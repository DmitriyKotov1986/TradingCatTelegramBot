#pragma once

//QT
#include <QObject>
#include <QThread>

//STL
#include <memory>
#include <list>

//My
#include <Common/tdbloger.h>

#include <Telegram/TelegramBotAPI.h>

#include <TradingCatCommon/usercore.h>

#include "config.h"
#include "users.h"

class Core final
    : public QObject
{
    Q_OBJECT

public:
    explicit Core(QObject *parent = nullptr);
    ~Core();

    QString errorString();
    bool isError() const;

public slots:
    void start();      //запуск работы
    void stop();

signals:
    //for Decector
    void setUserFilters(qint64 userId, const TradingCatCommon::Filter& filter);
    void eraseUserFilter(qint64 userId);

    void getServerStatus(qint64 userId);

    void stopAll();

private slots:
    void errorOccurredLoger(Common::EXIT_CODE errorCode, const QString& errorString);
    void errorOccurredUsers(Common::EXIT_CODE errorCode, const QString &errorString);
    void errorOccurredConfig(Common::EXIT_CODE errorCode, const QString &errorString);

    void errorOccurredBot(Telegram::Error error);
    void networkerrorOccurredBot(Telegram::Error error);
    void messageReceivedBot(qint32 update_id, Telegram::Message message);
    void callbackQueryReceivedBot(qint32 message_id, Telegram::CallbackQuery callback_query);
    void updateDataFromServer();

    void sendLogMsgUserCore(Common::TDBLoger::MSG_CODE category, const QString& msg);
    void klineDetectUserCore(const TradingCatCommon::Detector::PKLineDetectData& klineData, const TradingCatCommon::PKLinesList& klinesList);
    void orderBookDetectUserCore(const TradingCatCommon::Detector::POrderDetectData& orderData, const TradingCatCommon::PKLinesList& klinesLista);

    void serverStatusUserCore(qint64 chatId, const QString&serverName, const QString& serverVersion, const QDateTime& serverTime, qint64 upTime);

private:
    enum class UserEditAction: quint8
    {
        UNBLOCK = 0,
        BLOCK = 1
    };

private:
    Q_DISABLE_COPY_MOVE(Core);

    void sendMessage(const Telegram::FunctionArguments::SendMessage& arguments);
    void sendPhoto(const Telegram::FunctionArguments::SendPhoto& arguments);

    void initUser(qint64 chatId, const Telegram::Message& message);
    void removeUser(qint64 chatId, qint64 userId);
    void rebootUsers(const QString& userMessage);
    void setUserState(qint64 chatId, qint64 userId, UserTG::EUserState state);

    void startUsersEdit(qint64 chatId, qint64 userId);
    void usersSelectList(qint64 chatId, qint64 userId, UserEditAction action);
    void userConfirm(qint64 chatId, qint64 userId, qint64 userWorkId);
    void userBlock(qint64 chatId, qint64 userId, qint64 userWorkId);

    void cancel(qint64 chatId, qint64 userId);

    void startButton(qint64 chatId, UserTG::EUserRole role);
    void startUserButton(qint64 chatId);
    void startAdminButton(qint64 chatId);

    void clearButton(qint64 chatId);
    void cancelButton(qint64 chatId);

    void addDetectFiltersType(TradingCatCommon::Filter::FilterTypes filterActivate);
    void addDetectFilterType(TradingCatCommon::Filter::FilterType filterType);

    void sendBotStatus(qint64 chatId);

private:
    Config *_cnf = nullptr;                            //Конфигурация
    Common::TDBLoger *_loger = nullptr;

    QString _errorString;

    bool _isStarted = false;

    Telegram::Bot* _bot = nullptr;

    QTimer* _updateTimer = nullptr;

    Users* _users = nullptr;

    struct UserCoreThread
    {
        std::unique_ptr<TradingCatCommon::UserCore> userCore = nullptr;
        std::unique_ptr<QThread> thread;
    };

    using  PUserCoreThread = std::unique_ptr<UserCoreThread>;
    std::list<PUserCoreThread> _userCores;

    std::unordered_map<TradingCatCommon::Filter::FilterType, quint64> _detectFilterType;
    QDateTime _startDateTime = QDateTime::currentDateTime();

}; //class Core


