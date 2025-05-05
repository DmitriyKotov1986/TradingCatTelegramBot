//Qt
#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

//My
#include <Common/common.h>

#include <TradingCatCommon/filter.h>

#include "config.h"

using namespace TradingCatCommon;
using namespace Common;

//static
static Config* config_ptr = nullptr;
Q_GLOBAL_STATIC_WITH_ARGS(const QString, CONFIG_DB_NAME, ("Config"))
Q_GLOBAL_STATIC_WITH_ARGS(const QString, UPDATEID_PARAM_NAME, ("UpdateID"))

Config* Config::config(const QString& configFileName)
{
    if (config_ptr == nullptr)
    {
        config_ptr = new Config(configFileName);
    }

    return config_ptr;
};

void Config::deleteConfig()
{
    Q_CHECK_PTR(config_ptr);

    if (config_ptr != nullptr)
    {
        delete config_ptr;

        config_ptr= nullptr;
    }
}

//class
Config::Config(const QString& configFileName) :
    _configFileName(configFileName)
{
    if (_configFileName.isEmpty())
    {
        _errorString = "Configuration file name cannot be empty";

        return;
    }
    if (!QFileInfo(_configFileName).exists())
    {
        _errorString = "Configuration file not exist. File name: " + _configFileName;

        return;
    }

    qDebug() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg("Reading configuration from " +  _configFileName);

    QSettings ini(_configFileName, QSettings::IniFormat);

    QStringList groups = ini.childGroups();
    if (!groups.contains("DATABASE"))
    {
        _errorString = "Configuration file not contains [DATABASE] group";

        return;
    }
    if (!groups.contains("BOT"))
    {
        _errorString = "Configuration file not contains [BOT] group";

        return;
    }

    //Database
    ini.beginGroup("DATABASE");

    _dbConnectionInfo.driver = ini.value("Driver", "").toString();
    _dbConnectionInfo.dbName = ini.value("DataBase", "").toString();
    _dbConnectionInfo.userName = ini.value("UID", "").toString();
    _dbConnectionInfo.password = ini.value("PWD", "").toString();
    _dbConnectionInfo.connectOptions = ini.value("ConnectionOptions", "").toString();
    _dbConnectionInfo.port = ini.value("Port", "").toUInt();
    _dbConnectionInfo.host = ini.value("Host", "").toString();

    const auto errDataBase = _dbConnectionInfo.check();

    if (!errDataBase.isEmpty())
    {
        _errorString = QString("Incorrect value [DATABASE]. Error: %1").arg(errDataBase);

        return;
    }

    ini.endGroup();

    //SYSTEM
    ini.beginGroup("SYSTEM");

    _debugMode = ini.value("DebugMode", "0").toBool();
    _logTableName = ini.value("LogTableName", "").toString();
    QJsonParseError parseErr;
    _defaultFilter = QJsonDocument::fromJson(QByteArray::fromBase64(ini.value("DefaultFilter", "").toByteArray()), &parseErr).object();
    if (_defaultFilter.isEmpty())
    {
        _errorString = QString("Key value [SYSTEM]/DefaultFilter cannot be empty or parse error: %1").arg(parseErr.errorString());

        return;
    }
    else
    {
        Filter filter(_defaultFilter);
        if (filter.isError())
        {
            _errorString = QString("Key value [SYSTEM]/DefaultFilter has incorrect format: %1").arg(filter.errorString());

            return;
        }
    }

    ini.endGroup();

    //Bot
    ini.beginGroup("BOT");

    _botToken  = ini.value("Token", "").toString();
    if (_botToken.isEmpty())
    {
        _errorString = "Key value [BOT]/Token cannot be empty";

        return;
    }
    _botName  = ini.value("Name", "").toString();

    ini.endGroup();

    //[STOCK_EXCHAGE_N]


    //PROXY_N
    quint16 currentStockExchangeIndex = 0;
    for (const auto& group: groups)
    {
        if (group == QString("STOCK_EXCHAGE_SERVER_%1").arg(currentStockExchangeIndex))
        {
            ini.beginGroup(group);

            TradingCatCommon::HTTPClientConfig clientConfig;
            clientConfig.address = QHostAddress(ini.value("Host", "localhost").toString());
            if (clientConfig.address.isNull())
            {
                _errorString = QString("Value in [%1]/Host is not valid").arg(group);

                return;
            }

            bool ok = false;
            clientConfig.port = ini.value("Port", 0).toUInt(&ok);
            if (clientConfig.port == 0)
            {
                _errorString = QString("Value in [%1/Port must be number").arg(group);

                return;
            }

            ini.endGroup();

            _clientConfigsList.emplace_back(std::move(clientConfig));

            ++currentStockExchangeIndex;
        }
    }
    if (_clientConfigsList.empty())
    {
        _errorString = QString("at least one group [STOCK_EXCHAGE_SERVER_N] must be defined");

        return;
    }
}

const DBConnectionInfo &Config::dbConnectionInfo() const noexcept
{
    return _dbConnectionInfo;
}

bool Config::debugMode() const noexcept
{
    return _debugMode;
}

const QString &Config::logTableName() const noexcept
{
    return _logTableName;
}

const QJsonObject &Config::defaultFilter() const noexcept
{
    return _defaultFilter;
}

const HTTPClientConfigsList &Config::clientConfigsList() const noexcept
{
    return _clientConfigsList;
}

qint32 Config::bot_updateId()
{
    loadFromDB();

    auto valueStr = _dbConfig->getValue(*UPDATEID_PARAM_NAME);
    bool ok = false;
    qint32 result = valueStr.toInt(&ok);

    if (!ok)
    {
        qWarning() << QString("Key value [ConfigDB]/UpdateID must be number. Value: %1").arg(valueStr);

        set_bot_UpdateId(result);
    }

    return result;
}

void Config::set_bot_UpdateId(qint32 updateId)
{
    loadFromDB();

    Q_ASSERT(_dbConfig->getValue(*UPDATEID_PARAM_NAME).toInt() <= updateId);

    _dbConfig->setValue(*UPDATEID_PARAM_NAME, QString::number(updateId));
}

void Config::makeConfig(const QString& configFileName)
{
    if (configFileName.isEmpty())
    {
        qWarning() << "Configuration file name cannot be empty";

        return;
    }

    QSettings ini(configFileName, QSettings::IniFormat);

    if (!ini.isWritable())
    {
        qWarning() << QString("Can not write configuration file: %1").arg(configFileName);

        return;
    }

    ini.clear();

    //Database
    ini.beginGroup("DATABASE");

    ini.remove("");

    ini.setValue("Driver", "QMYSQL");
    ini.setValue("DataBase", "TradingCat");
    ini.setValue("UID", "user");
    ini.setValue("PWD", "password");
    ini.setValue("ConnectionOptions", "");
    ini.setValue("Port", "3306");
    ini.setValue("Host", "localhost");

    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    ini.remove("");

    ini.setValue("DebugMode", true);
    ini.setValue("LogTableName", QString("%1Log").arg(QCoreApplication::applicationName()));
    ini.setValue("DefaultFilter", "{}");

    ini.endGroup();

    //Bot
    ini.beginGroup("BOT");

    ini.remove("");

    ini.setValue("Token", "API token");
    ini.setValue("Name", QCoreApplication::applicationName());

    ini.endGroup();


    //STOCK_EXCHAGE_SERVER_N
    ini.beginGroup("STOCK_EXCHAGE_SERVER_0");

    ini.remove("");

    ini.setValue("Host", "localhost");
    ini.setValue("Port", 0);

    ini.endGroup();

    //сбрасываем буфер
    ini.sync();

    qInfo() << QString("Save configuration to %1").arg(configFileName);
}

QString Config::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void Config::errorOccurredDBConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    _errorString = errorString;

    emit errorOccurred(errorCode, errorString);
}

bool Config::isError() const noexcept
{
    return !_errorString.isEmpty();
}

void Config::loadFromDB()
{
    if (_dbConfig)
    {
        return;
    }

    _dbConfig = new TDBConfig(_dbConnectionInfo, *CONFIG_DB_NAME);

    QObject::connect(_dbConfig, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)), SLOT(errorOccurredDBConfig(Common::EXIT_CODE, const QString&)));

    if (_dbConfig->isError())
    {
        _errorString = _dbConfig->errorString();
    }
}
