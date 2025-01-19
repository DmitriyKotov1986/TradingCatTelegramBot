#pragma once

//QT
#include <QString>

//My
#include <Common/sql.h>
#include <Common/tdbconfig.h>
#include <TradingCatCommon/types.h>
#include <TradingCatCommon/stockexchange.h>
#include <TradingCatCommon/httpserver.h>
#include <TradingCatCommon/httpclient.h>

///////////////////////////////////////////////////////////////////////////////
/// The Config class - класс конфигурации приложения
///
class Config final
    : public QObject
{
    Q_OBJECT

public:
    /*!
        Обеспечивает Синглтон конфигурации. Метод возвращает указатель на класс конфигурации если configFileName пустая строка или
            создает новую, если указано имя файла.
        @param configFileName - имя фала конфигурации
        @return указатель на класс конфигурации
    */
    static Config* config(const QString& configFileName = QString());

    /*!
        Удаляет глобальный объект конфигурации. Этот метод следует вызывать в самом конце
    */
    static void deleteConfig();

    /*!
        Создает файл конфигурации со значениями по умолчанию. Если файл уже существует - он будет перезаписан
        @param configFileName - имя файла конфигурации
    */
    static void makeConfig(const QString& configFileName);

private:
    /*!
        Конструктор. Предполагается использование только этого конструктора
        @param configFileName - имя файла конфигурации. Файл должен существовать. В случае ошибки чтения файла isError() вернет true.
            Текстовое описание ошибки можно узнать errorString()
     */
    explicit Config(const QString& configFileName);

    /*!
        Производит загрузку параметров сохраненных в БД
    */
    void loadFromDB();

public:
    //[DATABASE]
    const Common::DBConnectionInfo& dbConnectionInfo() const noexcept;

    //[SYSTEM]
    bool debugMode() const noexcept;
    const QString& logTableName() const noexcept;

    //[BOT]
    const QString& botToken() const { return _botToken; }

    //[STOCK_EXCHAGE]
    /*!
        Возвращает список параметров подключения к серверам бирж Гарантируется что список не будет пустым
        @return список параметров подключения
     */
    const TradingCatCommon::HTTPClientConfigsList& clientConfigsList() const noexcept;

    //DBConfig
    qint32 bot_updateId();
    void set_bot_UpdateId(qint32 updateId);

    //errors
    /*!
        Возвращает true - если последний парсинг json закончился ошибкой
        @return true - была ошибка парсинга
    */
    bool isError() const noexcept;

    /*!
        Текстовое описание последней ошибки парсинга json. При вызове этого метода происходит сброс ошибки
        @return текст ошибки
    */
    [[nodiscard]] QString errorString();

signals:
    /*!
        Сигнал генерируется если произошла фатальная ошибка чтения/записи конфигурации из БД
        @param errorCode - код ошибки
        @param errorString - текстовое описание ошибки
    */
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);

private slots:
    void errorOccurredDBConfig(Common::EXIT_CODE errorCode, const QString& errorString);

private:
    const QString _configFileName;              ///< Имя файла конфигурации
    QString _errorString;                       ///< Текстовое описание последней ошибки
    Common::TDBConfig* _dbConfig = nullptr;     ///< Обеспечивает чтение/запись параметров хранящихся в БД

    //[SYSTEM]
    bool _debugMode = true;                     ///< Флаг отладочного вывода логов
    QString _logTableName;                      ///<  Имя таблиы логов.

    //[DATABASE]
    Common::DBConnectionInfo _dbConnectionInfo; ///< Параметры подключения к БД

    //[BOT]
    QString _botToken;                          ///< Токен телеграмм бота

    //[CLIENT]
    TradingCatCommon::HTTPClientConfigsList _clientConfigsList;  ///<  Список серверов бирж
};

