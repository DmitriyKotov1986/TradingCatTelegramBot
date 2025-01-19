#pragma once

//SLT
#include <unordered_set>

//Qt
#include <QObject>
#include <QDateTime>
#include <QUuid>
#include <QVariant>
#include <QJsonArray>

//My
#include <Common/common.h>

#include <TradingCatCommon/filter.h>

class UserTG
    : public QObject
{
    Q_OBJECT

public:
    /*!
        Роли пользователей
    */
    enum class EUserRole: quint8
    {
        UNDEFINED = 0,      ///< не определено
        NO_CONFIRMED = 1,   ///< пользователь зарегистрировался и ждет подтверждения админа
        USER = 2,           ///< пользовтель - обычный пользователь
        ADMIN = 3,          ///< пользователь - админ
        DELETED = 4         ///< пользователь удален или заблокирован
    };

    static EUserRole intToEUserRole(quint8 role);

    /*!
        Текущее состояния пользователя
    */
    enum class EUserState: quint8
    {
        UNDEFINED = 0,  //состояние неопределено
        BLOCKED = 1, //польователь заблокирован
        READY = 2, //готов к приему очередной команды. Это основное состояние пользователя
        USER_EDIT = 3 //редактирование пользователя
    };

    static EUserState intToEUserState(quint8 state);

    using  ChatsIdList = std::unordered_set<qint64>;

public:
    UserTG(qint64 telegramID,
         const QString& firstName,
         const QString& lastName,
         const QString& userName,
         EUserRole role,
         EUserState state,
         const TradingCatCommon::Filter& filter,
         QObject* parent = nullptr);

    ~UserTG();

    UserTG(const UserTG& user);
    UserTG& operator=(const UserTG& user);
    UserTG(UserTG&& user);
    UserTG& operator=(UserTG&& user);

    //user data
    qint64 telegramID() const { return  _telegramID; }
    const QString& firstName() const { return _firstName; }
    const QString& lastName() const { return _lastName; }
    const QString& userName() const { return _userName; }

    /*!
        Возвращает имя пользователя для отображения в различных запросах
        @return составное имя пользователя ТГ
    */
    QString getViewUserName() const;

    //role
    /*!
        Возвращает текущую роль пользователя
        @return роль пользователя
     */
    EUserRole role() const noexcept;

    /*!
        Устанавливает новую роль пользователя. Если роль пользователя изменилась - ганерируется сигнал roleChenged(...)
        @param role - новая роль пользователя. Не должно быть равно EUserRole::UNDEFINED
    */
    void setRole(EUserRole role);

    //chats
    /*!
        Добавляет чат с текущим пользователем. Если чат с chatId уже добавлен для данного пользователя - метод ничего не делает.
            При добавлении чата - метод гененрирует сигнал addNewChat(...)
        @param chatId - ИД чата, Не должно быть равно 0
    */
    void addChat(qint64 chatId);

    /*!
        Возвращает true - если чат с chatId уже у пользователя добавлен
        @param chatId - ИД чата
        @return - true - если чат с chatId уже у пользователя добавлен, иначе false
    */
    bool chatIsExist(qint64 chatId);

    /*!
        Возвращает список ИД чатов текущего пользователя
        @return список ИД чатов
    */
    const ChatsIdList& chatsIdList() const noexcept;

    //state
    /*!
        Возвращает текущее состояние пользователя
        @return состояние пользователя
     */
    EUserState state() const noexcept;
    /*!
        Устанавливает новое состояние пользователя. Если состояние пользователя изменилась - ганерируется сигнал stateChenged(...)
        @param state - новое состояние пользователя. Не должно быть равно EUserState::UNDEFINED)
     */
    void setState(EUserState state);

    //Filter
    /*!
        Возвращает текущий фильр для пользователя
        @return фильр пользователя
    */
    const TradingCatCommon::Filter& filter() const noexcept;

    /*!
        Устанавливает новый фильр для пользователя. Если старый и новый фильтр совпадают - метод ничего не делает.
            Если фильр изменился - генерируется сигнал filterChenged(...)
        @param filter - новый фильтр пользователя
     */
    void setFilter(const TradingCatCommon::Filter& filter);

private:
    UserTG() = delete;

signals:
    void roleChenged(qint64 userId, UserTG::EUserRole role);
    void stateChenged(qint64 userId, UserTG::EUserState state);
    void filterChenged(qint64 userId, TradingCatCommon::Filter& filter);
    void addNewChat(qint64 userId, qint64 chatId);

private:
    const qint64 _telegramID = 0;   ///< ИД пользователя телеграмма
    const QString _firstName;       ///< Имя пользователя ТГ. Может быть пустым
    const QString _lastName;        ///< Фамилия пользователя ТГ. Может быть пустым
    const QString _userName;        ///< Никнайм пользователя ТГ. Может быть пустым
    EUserRole _role = EUserRole::UNDEFINED;     ///< Роль пользователя
    EUserState _state = EUserState::UNDEFINED;  ///< Текущее состояние пользователя
    ChatsIdList _chats;                         ///< Список чатов с пользователем
    TradingCatCommon::Filter _filter;

};

Q_DECLARE_METATYPE(UserTG::EUserRole)
Q_DECLARE_METATYPE(UserTG::EUserState)
