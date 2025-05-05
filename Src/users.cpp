//STL
#include <stdexcept>

//Qt
#include <QSqlQuery>

//My
#include <Common/sql.h>
#include <Common/parser.h>

#include "users.h"

static const QString DB_CONNECTION_NAME = "UsersDB";

using namespace TradingCatCommon;
using namespace Common;

///////////////////////////////////////////////////////////////////////////////
/// class Users
Users::Users(const Common::DBConnectionInfo &dbConnectionInfo, const QJsonObject& defaultFilter, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _defaultFilter(defaultFilter)
{
}

Users::~Users()
{
    if (_db.isOpen())
    {
        closeDB(_db);
    }
}

bool Users::loadFromDB()
{
    try
    {
        connectToDB(_db, _dbConnectionInfo, DB_CONNECTION_NAME);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return false;
    }

    class LoadException
        : public std::runtime_error
    {
    public:
        LoadException(const QString& err)
            : std::runtime_error(err.toStdString())
        {}

    private:
        LoadException() = delete;

    };

    QStringList userIdList;
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        QString queryText =
            QString("SELECT `ID`, `TelegramID`, `FirstName`, `LastName`, `UserName`, `Role`, `State`, `Filter`"
                    "FROM `Users` "
                    "ORDER BY `ID` ");

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId = query.value("ID").toLongLong();
            const qint64 userId = query.value("TelegramID").toLongLong();
            if (userId <= 0)
            {
                throw LoadException(QString("User ID cannot be null or less in [Users]/TelegramID. Record: %1").arg(recordId));
            }

            const auto role = UserTG::intToEUserRole(query.value("Role").toUInt());
            if (role == UserTG::EUserRole::UNDEFINED)
            {
                throw LoadException(QString("User role cannot be UNDEFINE in [Users]/Role. Record: %1").arg(recordId));
            }

            const auto state = UserTG::intToEUserState(query.value("State").toUInt());
            if (state == UserTG::EUserState::UNDEFINED)
            {
                throw LoadException(QString("User state cannot be UNDEFINE in [Users]/State. Record: %1").arg(recordId));
            }

            const auto filterJson = query.value("Filter").toString().toUtf8();
            QJsonObject filterJsonDoc;
            try
            {
                filterJsonDoc = JSONParseToMap(QByteArray::fromBase64(filterJson));
            }
            catch (const ParseException& err)
            {
                throw LoadException(QString("User filter settings is incorrect: %1. Record: %2").arg(err.what()).arg(recordId));
            }

            Filter filter = filterJsonDoc.isEmpty() ? Filter(_defaultFilter) : Filter(filterJsonDoc);
            if (filter.isError())
            {
                throw LoadException(QString("User filter contain error in [Users]/Filter. %1. Record: %2").arg(filter.errorString()).arg(recordId));
            }

            auto user = UserTG(userId,
                               query.value("FirstName").toString(),
                               query.value("LastName").toString(),
                               query.value("UserName").toString(),
                               role,
                               state,
                               filter);

            userIdList.push_back(QString::number(userId));

            addUser(user);
        }

        //Пользовтелей совсем нет, больше нечего загружать....
        if (userIdList.isEmpty())
        {
            return true;
        }

        query.clear();

        queryText =
            QString("SELECT `ID`, `UserID`, `ChatID`"
                    "FROM `Chats` "
                    "WHERE `UserID` In (%1)")
                .arg(userIdList.join(u','));

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            const auto recordId = query.value("ID").toLongLong();
            const qint64 userId = query.value("UserID").toLongLong();
            if (userId <= 0)
            {
                throw LoadException(QString("User ID cannot be null or less in [Chats]/UserID. Record: %1").arg(recordId));
            }

            auto& user = _users.find(userId)->second;
            user->addChat(query.value("ChatID").toLongLong());
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {

        _users.clear();

        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return false;
    }
    catch (const LoadException& err)
    {
        _db.rollback();

        _users.clear();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, err.what());

        return false;
    }

    return true;
}

void Users::addNewUser(const UserTG& user)
{
    Q_ASSERT(user.telegramID() != 0);
    Q_ASSERT(user.role() != UserTG::EUserRole::UNDEFINED);
    Q_ASSERT(user.state() != UserTG::EUserState::UNDEFINED);

    if (_users.contains(user.telegramID()))
    {
        return;
    }

    const auto userId = user.telegramID();

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);

        auto queryText =
        QString("INSERT INTO `Users` (`TelegramID`, `FirstName`, `LastName`, `UserName`, `Role`, `State`, `Filter`) "
                "VALUES (%1, '%2', '%3', '%4', %5, %6, '%7')")
                               .arg(userId)
                               .arg(user.firstName())
                               .arg(user.lastName())
                               .arg(user.userName())
                               .arg(static_cast<quint8>(user.role()))
                               .arg(static_cast<quint8>(user.state()))
                               .arg(QJsonDocument(user.filter().toJson()).toJson().toBase64());


        DBQueryExecute(_db, query, queryText);

        for (const auto chatId: user.chatsIdList())
        {
            queryText =
                QString("INSERT INTO `Chats` (`UserID`, `ChatId`) "
                        "VALUES (%1, %2)")
                    .arg(userId)
                    .arg(chatId);

            DBQueryExecute(_db, query, queryText);
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }

    addUser(user);
}

void Users::removeUser(qint64 userId)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(_users.contains(userId));

    const auto& user = _users.find(userId)->second;

    user->setRole(UserTG::EUserRole::DELETED);
}

Users::UsersIDList Users::userIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        results.insert(user.first);
    }

    return results;
}

Users::UsersIDList Users::noConfirmUserIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        if (user.second->role() == UserTG::EUserRole::NO_CONFIRMED)
        {
            results.insert(user.first);
        }
    }

    return results;
}

Users::UsersIDList Users::confirmUserIdList() const
{
    UsersIDList results;

    for (const auto& user: _users)
    {
        const auto role = user.second->role();
        if (role == UserTG::EUserRole::USER || role == UserTG::EUserRole::ADMIN)
        {
            results.insert(user.first);
        }
    }

    return results;
}

UserTG& Users::user(qint64 userId) const
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(_users.contains(userId));

    return *_users.at(userId).get();
}

bool Users::userExist(qint64 userId) const
{
    Q_ASSERT(userId != 0);

    return _users.contains(userId);
}

quint64 Users::usersCount() const
{
    return _users.size();
}

void Users::updateUsersDB(qint64 userId, const QString& field, const QString& value)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(_users.contains(userId));

    const auto queryText = QString("UPDATE `Users` "
                                   "SET `%1` = '%2' "
                                   "WHERE `TelegramID` = %3")
                               .arg(field)
                               .arg(value)
                               .arg(userId);

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}

void Users::addUser(const UserTG& user)
{
    auto tmpUser(user);

    addUser(tmpUser);
}

void Users::filterChengedUser(qint64 userId, TradingCatCommon::Filter &filter)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(filter.isError());
    Q_ASSERT(_users.contains(userId));

    updateUsersDB(userId, "Filter", QJsonDocument(filter.toJson()).toJson().toBase64());
}

void Users::roleChengedUser(qint64 userId, UserTG::EUserRole role)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(role != UserTG::EUserRole::UNDEFINED);
    Q_ASSERT(_users.contains(userId));

    updateUsersDB(userId, "Role", QString::number(static_cast<quint8>(role)));
}

void Users::stateChengedUser(qint64 userId, UserTG::EUserState state)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(state != UserTG::EUserState::UNDEFINED);
    Q_ASSERT(_users.contains(userId));

    updateUsersDB(userId, "State", QString::number(static_cast<quint8>(state)));
}

void Users::addNewChatUser(qint64 userId, qint64 chatId)
{
    Q_ASSERT(userId != 0);
    Q_ASSERT(chatId != 0);
    Q_ASSERT(_users.contains(userId));
    Q_ASSERT(_users.at(userId).get()->chatIsExist(chatId));

    const auto queryText =
        QString("INSERT INTO `Chats` (`UserID`, `ChatId`) "
                "VALUES (%1, %2)")
            .arg(userId)
            .arg(chatId);

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }
}




