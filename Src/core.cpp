//QT
#include <QtNetwork/QNetworkProxy>
#include <QCoreApplication>

//My
#include <TradingCatCommon/klinechart.h>
#include "buttondata.h"

#include "core.h"

using namespace TradingCatCommon;
using namespace Telegram;
using namespace Common;

bool needExit = false;

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(Config::config())
    , _loger(TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    QObject::connect(_loger, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredLoger(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);

    QObject::connect(_cnf, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredConfig(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);
}

Core::~Core()
{
    stop();
}

void Core::start()
{
    Q_CHECK_PTR(_loger);
    Q_CHECK_PTR(_cnf);

    //Load users
    Q_ASSERT(_users == nullptr);
    _users = new Users(_cnf->dbConnectionInfo());

    QObject::connect(_users, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredUsers(Common::EXIT_CODE, const QString&)));

    if (!_users->loadFromDB())
    {
        return;
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Total users: %1").arg(_users->usersCount()));

    //Make UserCore
    Q_ASSERT(_userCores.empty());
    Q_ASSERT(!_cnf->clientConfigsList().empty());

    for (const auto& clientConfig: _cnf->clientConfigsList())
    {
        auto userCore = std::make_unique<UserCoreThread>();
        userCore->userCore = std::make_unique<UserCore>(clientConfig, KLineTypes{TradingCatCommon::KLineType::MIN1});
        userCore->thread = std::make_unique<QThread>();

        userCore->userCore->moveToThread(userCore->thread.get());

        connect(userCore->userCore.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
                SLOT(sendLogMsgUserCore(Common::TDBLoger::MSG_CODE, const QString&)), Qt::QueuedConnection);

        connect(userCore->userCore.get(),
                SIGNAL(klineDetect(const TradingCatCommon::Detector::UsersIdList&,
                                           const TradingCatCommon::StockExchangeID&,
                                           const TradingCatCommon::KLineID&,
                                           TradingCatCommon::Filter::FilterTypes,
                                           const TradingCatCommon::PKLinesList&)),
                SLOT(klineDetectUserCore(const TradingCatCommon::Detector::UsersIdList&,
                                         const TradingCatCommon::StockExchangeID&,
                                         const TradingCatCommon::KLineID&,
                                         TradingCatCommon::Filter::FilterTypes,
                                         const TradingCatCommon::PKLinesList&)), Qt::QueuedConnection);

        connect(userCore->userCore.get(), SIGNAL(serverStatus(qint64, const QString&, const QString&, const QDateTime&, qint64)),
                SLOT(serverStatusUserCore(qint64, const QString&, const QString&, const QDateTime&, qint64)), Qt::QueuedConnection);

        connect(this, SIGNAL(setUserFilters(qint64, const TradingCatCommon::Filter&)),
                userCore->userCore.get(), SLOT(setUserFilters(qint64, const TradingCatCommon::Filter&)), Qt::QueuedConnection);

        connect(this, SIGNAL(eraseUserFilter(qint64)), userCore->userCore.get(), SLOT(eraseUserFilter(qint64)), Qt::QueuedConnection);
        connect(this, SIGNAL(getServerStatus(qint64)), userCore->userCore.get(), SLOT(getServerStatus(qint64)), Qt::QueuedConnection);
        connect(this, SIGNAL(stopAll()), userCore->userCore.get(), SLOT(stop()), Qt::QueuedConnection);

        connect(userCore->thread.get(), SIGNAL(started()), userCore->userCore.get(), SLOT(start()), Qt::DirectConnection);
        connect(userCore->userCore.get(), SIGNAL(finished()), userCore->thread.get(), SLOT(quit()), Qt::DirectConnection);

        userCore->thread->start();

        _userCores.emplace_back(std::move(userCore));
    }

    for (const auto& userId: _users->confirmUserIdList())
    {
        const auto& user = _users->user(userId);
        emit setUserFilters(userId, user.filter());
    }

    //Make TG bot
    auto botSettings = std::make_shared<BotSettings>(_cnf->botToken());

    _bot = new Bot(botSettings);

    //	Telegram::Bot::errorOccurred is emitted every time when the negative response is received from the Telegram server and holds an Error object in arguments. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(errorOccurred(Telegram::Error)), SLOT(errorOccurredBot(Telegram::Error)));

    //	Telegram::Bot::networkerrorOccurred is emitted every time when there is an error while receiving Updates from the Telegram. Error class contains the occurred error description and code. See Error.h for details
    QObject::connect(_bot, SIGNAL(networkerrorOccurred(Telegram::Error)), SLOT(networkerrorOccurredBot(Telegram::Error)));

    // Connecting messageReceived() signal to a lambda that sends the received message back to the sender
    QObject::connect(_bot, SIGNAL(messageReceived(qint32, Telegram::Message)), SLOT(messageReceivedBot(qint32, Telegram::Message)));

    // Emited when bot reseive new incoming callback query */
    QObject::connect(_bot, SIGNAL(callbackQueryReceived(qint32, Telegram::CallbackQuery)), SLOT(callbackQueryReceivedBot(qint32, Telegram::CallbackQuery)));

    qDebug() << "Server bot data: " << _bot->getMe().get().toObject();	// To get basic data about your bot in form of a User object

    // Start update timer
    Q_ASSERT(_updateTimer == nullptr);
    _updateTimer = new QTimer();

    QObject::connect(_updateTimer, SIGNAL(timeout()), SLOT(updateDataFromServer()));

    _updateTimer->start(5000);

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Bot started successfully");

    rebootUsers(tr("The server has been rebooted by the administrator. Please start over"));

    _isStarted = true;

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Started successfully");
}

void Core::stop()
{
    if (!_isStarted)
    {
        return;
    }

    Q_CHECK_PTR(_loger);

    delete _updateTimer;
    _updateTimer = nullptr;

    delete _bot;
    _bot = nullptr;

    emit stopAll();
    for (const auto& userCore: _userCores)
    {
        userCore->thread->wait();
    }
    _userCores.clear();

    delete _users;
    _users = nullptr;

    _isStarted = false;

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Stoped successfully");
}

void Core::errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString)
{
    const auto msg = QString("Critical error while the Loger is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);

    qCritical() << msg;

    stop();

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredUsers(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Users is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    stop();

    QCoreApplication::exit(errorCode);
}

void Core::errorOccurredConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the Config is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;
    _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    stop();

    QCoreApplication::exit(errorCode);
}

void Core::sendMessage(const Telegram::FunctionArguments::SendMessage &arguments)
{
    Q_CHECK_PTR(_bot);

    _bot->sendMessage(arguments);

    QString chatIdStr;
    if (const auto chat_id_qint64 = std::get_if<qint64>(&arguments.chat_id))
    {
        chatIdStr = QString::number(*chat_id_qint64);
    }
    else if (const auto chat_id_string = std::get_if<QString>(&arguments.chat_id))
    {
        chatIdStr = *chat_id_string;
    }

    auto msg = QString("Chat ID: %1 Text: '%2'").arg(chatIdStr).arg(arguments.text);

    if (arguments.reply_markup.has_value())
    {
        const auto& replyMarkup = arguments.reply_markup.value();

        if (const auto buttonsList = std::get_if<InlineKeyboardMarkup>(&replyMarkup))
        {
            msg += QString(" Inline buttons: [");
            qint32 line = 1;
            for (const auto& buttonsGroup: buttonsList->inline_keyboard)
            {
                msg += QString("Line %1:(").arg(line);
                for (const auto& button: buttonsGroup)
                {
                    msg += QString("Text: '%1' Data: %2;").arg(button.text).arg(button.callback_data.has_value() ? button.callback_data.value() : "");
                }
                msg.removeLast();
                msg += QString(");");
                ++line;
            }
            msg.removeLast();
            msg += QString("]");
        }
        else if (const auto buttonsList = std::get_if<ReplyKeyboardMarkup>(&replyMarkup))
        {
            msg += QString(" ReplyKeyboard buttons: [");
            qint32 line = 1;
            for (const auto& buttonsGroup: buttonsList->keyboard)
            {
                msg += QString("Line %1:(").arg(line);
                for (const auto& button: buttonsGroup)
                {
                    msg += QString("Text: %1;").arg(button.text);
                }
                msg.removeLast();
                msg += QString(");");
                ++line;
            }
            msg.removeLast();
            msg += QString("]");
        }
        else if (const auto buttonsList = std::get_if<ReplyKeyboardRemove>(&replyMarkup))
        {
            msg += QString(" ReplyKeyboardRemove");
        }
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Send to user: %1").arg(msg));
}

void Core::sendPhoto(const Telegram::FunctionArguments::SendPhoto &arguments)
{
    _bot->sendPhoto(arguments);

    QString fileName;
    if (const auto qfile_fileName = std::get_if<QFile*>(&arguments.photo))
    {
        if (*qfile_fileName)
        {
            fileName = (*qfile_fileName)->fileName();
        }
    }
    else if (const auto string_fileName = std::get_if<QString>(&arguments.photo))
    {
        fileName = *string_fileName;
    }

    QString chatIdStr;
    if (const auto chat_id_qint64 = std::get_if<qint64>(&arguments.chat_id))
    {
        chatIdStr = QString::number(*chat_id_qint64);
    }
    else if (const auto chat_id_string = std::get_if<QString>(&arguments.chat_id))
    {
        chatIdStr = *chat_id_string;
    }

    const auto msg = QString("Chat ID %1: Photo from: %2. Caption: %3").arg(chatIdStr).arg(fileName).arg(arguments.caption.value_or(""));

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Send to user: %1").arg(msg));
}


void Core::errorOccurredBot(Telegram::Error error)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE,
                       QString("Got negative response is received from the Telegram server. Code: %1. Message: %2")
                           .arg(error.error_code)
                           .arg(error.description));
}

void Core::networkerrorOccurredBot(Telegram::Error error)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE,
                       QString("Got is an error while receiving Updates from the Telegram server. Code: %1. Message: %2")
                           .arg(error.error_code)
                           .arg(error.description));
}

void Core::messageReceivedBot(qint32 update_id, Telegram::Message message)
{
    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Get message from: %1. Update ID: %2. Message: %3")
                                                                 .arg(message.from->id)
                                                                 .arg(update_id)
                                                                 .arg(message.text.has_value() ? message.text.value() : "NO TEXT"));

    const auto userId = message.from->id;
    const auto chatId = message.chat->id;

    if (!message.text.has_value())
    {
        sendMessage({.chat_id = message.chat->id,
                     .text = tr("Support only text command")});

        return;
    }

    const auto& cmd = message.text.value();

    //команды доступные неавторизированным пользователям
    if (cmd == "/start")
    {
        initUser(chatId, message);

        return;
    }
    else  if (cmd == "/help")
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Support commands:\n/start - start bot\n/stop - stop bot\n/help - this help")});

        return;
    }

    //Проверяем есть ли такой пользователь
    if (!_users->userExist(userId) || _users->user(userId).role() == UserTG::EUserRole::DELETED)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Unknow command. Please send the /start command to start and send message to Administrator")});

        return;
    }

    //Команды доступные
    if (cmd == "/stop")
    {
        removeUser(chatId, userId);

        return;
    }

    //Проверяем активен ли пользователь
    const auto role = _users->user(userId).role();
    if (role != UserTG::EUserRole::USER && role != UserTG::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Authorization has not been completed. Try letter")});

        return;
    }

    //Проверяем активен есть ли права админа
    if (role == UserTG::EUserRole::ADMIN)
    {
        if (cmd == tr("Users"))
        {
            startUsersEdit(chatId, userId);

            return;
        }
        if (cmd == "/status")
        {
            emit getServerStatus(userId);

            return;
        }
        else if (cmd == tr("Cancel"))
        {
            cancel(chatId, userId);

            return;
        }
    }

    sendMessage({.chat_id = chatId,
                 .text = tr("Unsupport command")});
}

void Core::callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query)
{
    Q_UNUSED(message_id);

    _bot->answerCallbackQuery(callback_query.id);

    if (!callback_query.data.has_value())
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("User press undefine inline button. Skip. User: %1. Button text: %2")
                                                                 .arg(callback_query.from.id)
                                                                 .arg((callback_query.message.has_value() && callback_query.message->text.has_value()) ? callback_query.message->text.value() : ""));
        return;
    }

    const auto data = ButtonData(callback_query.data.value());

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User press inline button. User: %1. Button text: %2. Button data: %3")
                                                                 .arg(callback_query.from.id)
                                                                 .arg((callback_query.message.has_value() && callback_query.message->text.has_value()) ? callback_query.message->text.value() : "")
                                                                 .arg(data.toString()));

    const auto buttonName = data.getParam("N");
    if (buttonName == "USEREDIT")
    {
        const auto userId = data.getParam("U").toLongLong();
        const auto chatId = data.getParam("C").toLongLong();
        auto& user = _users->user(userId);
        if (user.state() != UserTG::EUserState::USER_EDIT)
        {
            sendMessage({.chat_id = chatId,
                         .text = tr("This button is not active. Please click 'Users' on start menu for edit users")});

            return;
        }

        const auto action_int = data.getParam("A").toUInt();
        UserEditAction action = UserEditAction::BLOCK;;
        switch (action_int)
        {
        case static_cast<quint8>(UserEditAction::UNBLOCK):
            action = UserEditAction::UNBLOCK;
            break;
        case static_cast<quint8>(UserEditAction::BLOCK):
            action = UserEditAction::BLOCK;
            break;
        default:
            Q_ASSERT(false);
        }

        const auto userWorkId = data.getParam("E");
        if (userWorkId.isEmpty())
        {
            usersSelectList(chatId, userId, action);
        }
        else
        {
            switch (action)
            {
            case UserEditAction::UNBLOCK:
                userConfirm(chatId, userId, userWorkId.toLongLong());
                break;
            case UserEditAction::BLOCK:
                userBlock(chatId, userId, userWorkId.toLongLong());
                break;
            default:
                Q_ASSERT(false);
            }
        }
    }

    else
    {
        Q_ASSERT(false);
    }
}

void Core::updateDataFromServer()
{
    Q_ASSERT(_isStarted);
    Q_CHECK_PTR(_bot);

    const auto newUpdateId = _bot->update(_cnf->bot_updateId());

    if (newUpdateId != _cnf->bot_updateId())
    {
        _cnf->set_bot_UpdateId(newUpdateId);
    }
}

void Core::sendLogMsgUserCore(Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    _loger->sendLogMsg(category, QString("User core: %1").arg(msg));
}

void Core::klineDetectUserCore(const TradingCatCommon::Detector::UsersIdList &usersId,
                               const TradingCatCommon::StockExchangeID &stockExchangeId,
                               const TradingCatCommon::KLineID &klineId,
                               TradingCatCommon::Filter::FilterTypes filterActivate,
                               const TradingCatCommon::PKLinesList &klinesList)
{
    Q_ASSERT(!stockExchangeId.isEmpty());
    Q_ASSERT(!klineId.isEmpty());

    QStringList usersIdlist;
    for (const auto& userId: usersId)
    {
        usersIdlist.push_back(QString::number(userId));
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Detect kline for users: %1. StockExchangeID: %2. KLineID: %3. Filter: %4")
        .arg(usersIdlist.join(','))
        .arg(stockExchangeId.toString())
        .arg(klineId.toString())
        .arg(filterActivate.toInt()));

    QStringList typeStr;
    if (filterActivate.testFlag(Filter::FilterType::DELTA))
    {
        typeStr.push_back(tr("Delta"));
    }
    if (filterActivate.testFlag(Filter::FilterType::VOLUME))
    {
        typeStr.push_back(tr("Volume"));
    }
    if (filterActivate.testFlag(Filter::FilterType::CLOSE_NATR_POSITIVE))
    {
        typeStr.push_back(tr("NATR+"));
    }
    if (filterActivate.testFlag(Filter::FilterType::CLOSE_NATR_NEGATIVE))
    {
        typeStr.push_back(tr("NATR-"));
    }
    if (filterActivate.testFlag(Filter::FilterType::VOLUME_NATR))
    {
        typeStr.push_back(tr("NATR_Volume"));
    }

    const auto msg = tr("Detect %1: %2 %3").arg(typeStr.join(',')).arg(stockExchangeId.toString()).arg(klineId.symbol);

    const auto chartFileName = QString("%1/%2/%3_%4_%5.png")
                                   .arg(QDir::tempPath())
                                   .arg(QCoreApplication::applicationName())
                                   .arg(stockExchangeId.toString())
                                   .arg(klineId.symbol)
                                   .arg(QDateTime::currentDateTime().toString(SIMPLY_DATETIME_FORMAT));
    KLineChart chart;
    chart.makeChart(chartFileName, stockExchangeId, klinesList);

    QFile chartImg(chartFileName);

    for (const auto& userId: usersId)
    {
        const auto& user = _users->user(userId);

        for (const auto& chatId: user.chatsIdList())
        {
            chartImg.open(QFile::ReadOnly);

            sendPhoto({.chat_id = chatId,
                       .photo = &chartImg,
                       .caption = msg});

            chartImg.close();
        }
    }
}

void Core::serverStatusUserCore(qint64 userId, const QString &serverName, const QString &serverVersion, const QDateTime &serverTime, qint64 upTime)
{
    const auto& user = _users->user(userId);

    const auto msg = QString("Server name: %1\nServer version: %2\nUptime: %3\nServer time: %4")
                         .arg(serverName)
                         .arg(serverVersion)
                         .arg(upTime)
                         .arg(serverTime.toString(SIMPLY_DATETIME_FORMAT));

    for (const auto& chatId: user.chatsIdList())
    {
        sendMessage({.chat_id = chatId,
                     .text = msg});
    }
}

void Core::initUser(qint64 chatId, const Telegram::Message& message)
{
    const auto userId = message.from->id;
    if (_users->userExist(userId))
    {
        auto& user = _users->user(userId);
        const auto role = user.role();
        switch (role)
        {
        case UserTG::EUserRole::USER:
        case UserTG::EUserRole::ADMIN:
            startButton(chatId, role);
            break;
        case UserTG::EUserRole::NO_CONFIRMED:
            sendMessage({.chat_id = chatId,
                         .text = tr("Please wait for account confirmation from the administrator")});
            break;
        case UserTG::EUserRole::DELETED:
            sendMessage({.chat_id = chatId,
                         .text = tr("Please wait for account unblocked from the administrator")});
            user.setRole(UserTG::EUserRole::NO_CONFIRMED);
            break;
        case UserTG::EUserRole::UNDEFINED:
        default:
            Q_ASSERT(false);
        }

        if (!user.chatIsExist(chatId))
        {
            user.addChat(chatId);
        }

        return;
    }

    auto user = UserTG(userId,
                       message.from->first_name,
                       message.from->last_name.has_value() ? message.from->last_name.value() : "",
                       message.from->username.has_value() ? message.from->username.value() : "",
                       UserTG::EUserRole::NO_CONFIRMED,
                       UserTG::EUserState::BLOCKED,
                       Filter::defaultFilter({}));

    user.addChat(chatId);

    _users->addNewUser(user);

    sendMessage({.chat_id = chatId,
                 .text = tr("Welcome to MOEX Telegram bot. Please wait for account confirmation from the administrator")});
}

void Core::removeUser(qint64 chatId, qint64 userId)
{
    _users->removeUser(userId);

    // Sending reply keyboard
    clearButton(chatId);
}

void Core::rebootUsers(const QString& userMessage)
{
    for (const auto& userId: _users->userIdList())
    {
        const auto& user = _users->user(userId);
        const auto role = user.role();
        if (role == UserTG::EUserRole::USER || role == UserTG::EUserRole::ADMIN)
        {
            if (user.state() != UserTG::EUserState::READY)
            {
                for (const auto& chatId: user.chatsIdList())
                {
                    sendMessage({.chat_id = chatId,
                                 .text = userMessage});
                    setUserState(chatId, userId, UserTG::EUserState::READY);
                }
            }
        }
    }
}

void Core::startButton(qint64 chatId, UserTG::EUserRole role)
{
    Q_ASSERT(role != UserTG::EUserRole::UNDEFINED);

    switch (role)
    {
    case UserTG::EUserRole::USER:
        startUserButton(chatId);
        break;
    case UserTG::EUserRole::ADMIN:
        startAdminButton(chatId);
        break;
    default:
        Q_ASSERT(false);
        break;
    }
}

void Core::cancel(qint64 chatId, qint64 userId)
{
    auto& user = _users->user(userId);

    switch (user.state())
    {
    case UserTG::EUserState::USER_EDIT:
    case UserTG::EUserState::READY:
    case UserTG::EUserState::BLOCKED:
    case UserTG::EUserState::UNDEFINED:
    default:
        break;
    }

    setUserState(chatId, userId, UserTG::EUserState::READY);
}

void Core::startUserButton(qint64 chatId)
{
  /*  KeyboardButton settingsButton(tr("Settings"));

    ReplyKeyboardMarkup keyboard = {{startButton}};

    // Sending reply keyboard
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click 'Start' button for new check AZS"),
                 .reply_markup = keyboard});*/
}

void Core::startAdminButton(qint64 chatId)
{
    KeyboardButton usersButton(tr("Users"));

    ReplyKeyboardMarkup keyboard = {{usersButton}};

    // Sending reply keyboard
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click: 'Users' for edit users data\n"),
                 .reply_markup = keyboard});
}

void Core::clearButton(qint64 chatId)
{
    sendMessage({.chat_id = chatId,
                 .text = tr("Goodbye"),
                 .reply_markup = ReplyKeyboardRemove()});
}

void Core::cancelButton(qint64 chatId)
{
    KeyboardButton cancelButton(tr("Cancel"));

    ReplyKeyboardMarkup keyboard = {{cancelButton}};
    //  keyboard .resize_keyboard = true;

    sendMessage({.chat_id = chatId,
                 .text = tr("Please click 'Cancel' for cancel"),
                 .reply_markup = keyboard });
}

void Core::startUsersEdit(qint64 chatId, qint64 userId)
{
    auto& user = _users->user(userId);
    if (user.role() != UserTG::EUserRole::ADMIN)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Insufficient rights to perform the action")});
        return;
    }

    setUserState(chatId, userId, UserTG::EUserState::USER_EDIT);

    QVector<InlineKeyboardButton> questionButtons;
    {
        ButtonData unblockButtonData;
        unblockButtonData.setParam("N", "USEREDIT");
        unblockButtonData.setParam("A", QString::number(static_cast<quint8>(UserEditAction::UNBLOCK)));
        unblockButtonData.setParam("U", QString::number(userId));
        unblockButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton unblockButton(tr("Unblock"), std::nullopt, std::nullopt, unblockButtonData.toString());
        questionButtons.push_back(unblockButton);
    }
    {
        ButtonData blockButtonData;
        blockButtonData.setParam("N", "USEREDIT");
        blockButtonData.setParam("A", QString::number(static_cast<quint8>(UserEditAction::BLOCK)));
        blockButtonData.setParam("U", QString::number(userId));
        blockButtonData.setParam("C", QString::number(chatId));

        InlineKeyboardButton blockButton(tr("Block"), std::nullopt, std::nullopt, blockButtonData.toString());

        questionButtons.push_back(blockButton);
    }

    QVector<QVector<InlineKeyboardButton>> buttons;
    buttons.push_back(questionButtons);

    // Sending all inline buttons
    sendMessage({.chat_id = chatId,
                 .text = tr("Please click to 'Unblock' for confirmed user registration or 'Block' for user blocked and delete"),
                 .reply_markup = buttons});
}

void Core::usersSelectList(qint64 chatId, qint64 userId, UserEditAction action)
{
    Users::UsersIDList userList;

    switch (action)
    {
    case UserEditAction::UNBLOCK:
        userList = _users->noConfirmUserIdList();
        break;
    case UserEditAction::BLOCK:
        userList = _users->confirmUserIdList();
        break;
    default:
        Q_ASSERT(false);
    }

    QVector<QVector<InlineKeyboardButton>> buttons;
    QVector<InlineKeyboardButton> questionButtons;

    for (const auto userWorkId: userList)
    {
        if (userWorkId == userId)
        {
            continue;
        }

        const auto& userWork = _users->user(userWorkId);

        ButtonData data;
        data.setParam("N", "USEREDIT");
        data.setParam("U", QString::number(userId));
        data.setParam("C", QString::number(chatId));
        data.setParam("A", QString::number(static_cast<quint8>(action)));
        data.setParam("E", QString::number(userWork.telegramID()));

        InlineKeyboardButton questionButton(userWork.getViewUserName(), std::nullopt, std::nullopt, data.toString());

        questionButtons.push_back(questionButton);

        if (questionButtons.size() == 2)
        {
            buttons.push_back(questionButtons);
            questionButtons.clear();
        }
    }

    if (!questionButtons.empty())
    {
        buttons.push_back(questionButtons);
    }

    InlineKeyboardMarkup inlineButtons(buttons);

    if (inlineButtons.isEmpty())
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("No user for edit")});

        setUserState(chatId, userId, UserTG::EUserState::READY);

        return;
    }

    // Sending all inline buttons
    sendMessage({.chat_id = chatId,
                 .text = tr("Please select user"),
                 .reply_markup = inlineButtons});
}

void Core::userConfirm(qint64 chatId, qint64 userId, qint64 userWorkId)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(userWorkId != 0);

    auto& userWork = _users->user(userWorkId);
    userWork.setRole(UserTG::EUserRole::USER);
    userWork.setState(UserTG::EUserState::READY);

    for (const auto& chatWorkID: userWork.chatsIdList())
    {
        sendMessage({.chat_id = chatWorkID,
                     .text = tr("Congratulations! Administrator has added you to the list of confirmed users")});

        setUserState(userWorkId, chatWorkID, UserTG::EUserState::READY);
    }

    emit setUserFilters(userWorkId, userWork.filter());

    sendMessage({.chat_id = chatId,
                 .text = tr("User %1 successfully confirmed").arg(userWork.userName())});

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User %1 successfully confirmed").arg(userWork.userName()));

    setUserState(chatId, userId, UserTG::EUserState::READY);
}

void Core::userBlock(qint64 chatId, qint64 userId, qint64 userWorkId)
{
    Q_ASSERT(chatId != 0);
    Q_ASSERT(userId != 0);
    Q_ASSERT(userWorkId != 0);

    auto& userWork = _users->user(userWorkId);

    for (const auto& chatWorkID: userWork.chatsIdList())
    {
        sendMessage({.chat_id = chatWorkID,
                     .text = tr("Administrator has deleted you from the list of confirmed users. Please send the /start command to start and send message to Administrator")});
        setUserState(userWorkId, chatWorkID, UserTG::EUserState::BLOCKED);
    }

    emit eraseUserFilter(userWorkId);

    userWork.setRole(UserTG::EUserRole::DELETED);

    sendMessage({.chat_id = chatId,
                 .text = tr("User %1 successfully deleted").arg(userWork.userName())});

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("User %1 successfully delete").arg(userWork.userName()));

    setUserState(chatId, userId, UserTG::EUserState::READY);
}

void Core::setUserState(qint64 chatId, qint64 userId, UserTG::EUserState state)
{
    Q_ASSERT(state != UserTG::EUserState::UNDEFINED);
    Q_ASSERT(_users->userExist(userId));

    auto& user =_users->user(userId);
    const auto role = user.role();

    if (role != UserTG::EUserRole::USER && role != UserTG::EUserRole::ADMIN && role != UserTG::EUserRole::NO_CONFIRMED)
    {
        sendMessage({.chat_id = chatId,
                     .text = tr("Authorization has not been completed. Please enter the /start command to start and send message to Administrator")});

        return;
    }

    if (role == UserTG::EUserRole::NO_CONFIRMED)
    {
        clearButton(chatId);

        return;
    }

    switch (state) {
    case UserTG::EUserState::READY:
        startButton(chatId, user.role());
        break;
    case UserTG::EUserState::BLOCKED:
        clearButton(chatId);
        break;
    case UserTG::EUserState::USER_EDIT:
        cancelButton(chatId);
        break;
    case UserTG::EUserState::UNDEFINED:
    default:
        Q_ASSERT(false);
        break;
    }

    user.setState(state);
}

QString Core::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

bool Core::isError() const
{
    return !_errorString.isEmpty();
}

