//My
#include "Common/parser.h"

#include "user.h"

static const QString DB_CONNECTION_NAME = "UsersDB";

using namespace Common;

///////////////////////////////////////////////////////////////////////////////
/// class User
UserTG::EUserRole UserTG::intToEUserRole(quint8 role)
{
    switch (role)
    {
    case static_cast<quint8>(EUserRole::ADMIN): return EUserRole::ADMIN;
    case static_cast<quint8>(EUserRole::USER): return EUserRole::USER;
    case static_cast<quint8>(EUserRole::DELETED): return EUserRole::DELETED;
    case static_cast<quint8>(EUserRole::NO_CONFIRMED): return EUserRole::NO_CONFIRMED;
    case static_cast<quint8>(EUserRole::UNDEFINED):
    default:
        break;
    }

    return EUserRole::UNDEFINED;
}

UserTG::EUserState UserTG::intToEUserState(quint8 state)
{
    switch (state)
    {
    case static_cast<quint8>(EUserState::READY): return EUserState::READY;
    case static_cast<quint8>(EUserState::BLOCKED): return EUserState::BLOCKED;
    case static_cast<quint8>(EUserState::USER_EDIT): return EUserState::USER_EDIT;
    case static_cast<quint8>(EUserState::UNDEFINED):
    default:
        break;
    }

    return EUserState::UNDEFINED;
}

UserTG::UserTG(qint64 telegramID,
           const QString &firstName,
           const QString& lastName,
           const QString& userName,
           EUserRole role,
           EUserState state,
           const TradingCatCommon::Filter& filter,
           QObject* parent /* = nullptr */)
    : QObject{parent}
    , _telegramID(telegramID)
    , _firstName(firstName)
    , _lastName(lastName)
    , _userName(userName)
    , _role(role)
    , _state(state)
    , _filter(filter)
{
    Q_ASSERT(_telegramID != 0);
    Q_ASSERT(_role != EUserRole::UNDEFINED);
    Q_ASSERT(_state != EUserState::UNDEFINED);
    Q_ASSERT(!_filter.isError());

    qRegisterMetaType<UserTG::EUserRole>("UserTG::EUserRole");
    qRegisterMetaType<UserTG::EUserState>("UserTG::EUserState");
}

UserTG::~UserTG()
{
}

UserTG::UserTG(const UserTG& user)
    : UserTG(user.telegramID(), user.firstName(), user.lastName(), user.userName(), user.role(), user.state(), user.filter(), user.parent())
{
}

UserTG &UserTG::operator=(const UserTG& user)
{
    if (this != &user)
    {
        UserTG tmpUser(user);
        std::swap(*this, tmpUser);
    }

    return *this;
}

UserTG::UserTG(UserTG&& user)
    : _telegramID(std::move(user._telegramID))
    , _firstName(std::move(user._firstName))
    , _lastName(std::move(user._lastName))
    , _userName(std::move(user._userName))
    , _role(std::move(user._role))
    , _state(std::move(user._state))
    , _chats(std::move(user._chats))
    , _filter(std::move(user._filter))
{
}

UserTG& UserTG::operator=(UserTG&& user)
{
    if (this != &user)
    {
        UserTG tmpUser(std::move(user));
        std::swap(*this, tmpUser);
    }

    return *this;
}

QString UserTG::getViewUserName() const
{
    auto userName = _userName;
    if (userName.isEmpty())
    {
        userName = _firstName;
    }
    if (userName.isEmpty())
    {
        userName = _lastName;
    }
    if (userName.isEmpty())
    {
        userName = QString::number(_telegramID);
    }

    return userName;
}

::UserTG::EUserRole UserTG::role() const noexcept
{
    return _role;
}

UserTG::EUserState UserTG::state() const noexcept
{
    return _state;
}

void UserTG::setRole(EUserRole role)
{
    Q_ASSERT(role != EUserRole::UNDEFINED);

    if (_role == role)
    {
        return;
    }

    _role = role;

    emit roleChenged(_telegramID, _role);
}

void UserTG::addChat(qint64 chatId)
{
    Q_ASSERT(chatId != 0);

    if (chatIsExist(chatId))
    {
        return;
    }

    _chats.insert(chatId);

    emit addNewChat(_telegramID, chatId);
}

bool UserTG::chatIsExist(qint64 chatId)
{
    Q_ASSERT(chatId != 0);

    return _chats.contains(chatId);
}

const UserTG::ChatsIdList& UserTG::chatsIdList() const noexcept
{
    return _chats;
}

void UserTG::setState(EUserState state)
{
    Q_ASSERT(state != EUserState::UNDEFINED);

    if (_state == state)
    {
        return;
    }

    _state = state;

    emit stateChenged(_telegramID, _state);
}

const TradingCatCommon::Filter &UserTG::filter() const noexcept
{
    return _filter;
}

void UserTG::setFilter(const TradingCatCommon::Filter& filter)
{
    Q_ASSERT(!filter.isError());

    _filter = filter;

    emit filterChenged(_telegramID, _filter);
}

