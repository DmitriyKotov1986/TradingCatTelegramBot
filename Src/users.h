#pragma once

//STL
#include <unordered_map>
#include <unordered_set>
#include <memory>

//Qt
#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QUuid>
#include <QJsonObject>

//My
#include <Common/common.h>
#include <Common/sql.h>

#include "user.h"

class Users final
    : public QObject
{
    Q_OBJECT

public:
    using UsersIDList = std::unordered_set<qint64>;

public:
    explicit Users(const Common::DBConnectionInfo& dbConnectionInfo, const QJsonObject& defaultFilter, QObject* parent = nullptr);
    ~Users();

    bool loadFromDB();

    void addNewUser(const UserTG& user);
    void removeUser(qint64 userId);

    UsersIDList userIdList() const;
    UsersIDList noConfirmUserIdList() const;
    UsersIDList confirmUserIdList() const;

    UserTG& user(qint64 userId) const;

    bool userExist(qint64 userId) const;

    quint64 usersCount() const;

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString &errorString);

    void filterChenged(qint64 userId, TradingCatCommon::Filter& filter);

private slots:
    void roleChengedUser(qint64 userId, UserTG::EUserRole role);
    void stateChengedUser(qint64 userId, UserTG::EUserState state);
    void filterChengedUser(qint64 userId, TradingCatCommon::Filter& filter);
    void addNewChatUser(qint64 userId, qint64 chatId);

private:
    Users() = delete;
    Q_DISABLE_COPY_MOVE(Users);

    void updateUsersDB(qint64 userId, const QString& field, const QString& value);

    template <class TUser>
    inline void addUser(TUser& user);

    void addUser(const UserTG& user);

private:
    const Common::DBConnectionInfo& _dbConnectionInfo;

    QSqlDatabase _db;

    std::unordered_map<qint64, std::unique_ptr<UserTG>> _users;

    QJsonObject _defaultFilter;

};

template <class TUser>
inline void Users::addUser(TUser& user)
{
    auto userLocal = std::forward<UserTG>(user);
    auto p_userLocal = std::make_unique<UserTG>(std::move(userLocal));

    connect(p_userLocal.get(), SIGNAL(roleChenged(qint64, UserTG::EUserRole)), SLOT(roleChengedUser(qint64, UserTG::EUserRole)));
    connect(p_userLocal.get(), SIGNAL(stateChenged(qint64, UserTG::EUserState)), SLOT(stateChengedUser(qint64, UserTG::EUserState)));
    connect(p_userLocal.get(), SIGNAL(filterChenged(qint64, TradingCatCommon::Filter&)), SLOT(filterChengedUser(qint64, TradingCatCommon::Filter&)));
    connect(p_userLocal.get(), SIGNAL(addNewChat(qint64, qint64)), SLOT(addNewChatUser(qint64, qint64)));

    auto id = p_userLocal->telegramID();
    _users.emplace(std::move(id), std::move(p_userLocal));
}
