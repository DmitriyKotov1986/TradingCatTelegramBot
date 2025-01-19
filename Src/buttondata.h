#pragma once

//Qt
#include <QHash>
#include <QString>

///////////////////////////////////////////////////////////////////////////////
/// The ButtonData class - Контейнер данных для кнопок
///
class ButtonData
{
public:
    /*!
        Конструтор
        @param paramStr - строка с параметрами. Ожидается формат:
            [param1]=[value1];[param2]=[value2] и т.д.
     */
    ButtonData(const QString& paramStr = QString());

    /*!
        Добавляет параметр с именем paramName и значением paramValue в контейнер
        @param paramName - название параметра. Значение на должно быть пустым
        @param paramValue - значение параметра
    */
    void setParam(const QString& paramName, const QString& paramValue);

    /*!
        Возвращает значение параметр с именем paramName. Если параметра и с именем paramName нет в контейнере
            возвращается пустая строка
        @param paramName - название параметра.
        @return значение параметра
    */
    const QString& getParam(const QString& paramName) const noexcept;

    /*!
        Преобразует строку в набор параметров и сохраняет их в контейнере. Сохраненные ранее параметры - удаляются
        @param paramStr - строка с параметрами. Ожидается формат:
            [param1]=[value1];[param2]=[value2] и т.д.
     */
    void fromString(const QString& paramStr);

    /*!
        Преобразует данный контейнер в строку
        @return строковое представление контейнера
     */
    QString toString() const;

    /*!
        Возвращает true если параметр с именем paramName существует
        @param paramName - название параметра
        @return true - если параметр с именем paramName существует, false - иначе
    */
    bool isExist(const QString& paramName) const noexcept;

    /*!
        Возвращает true если контейнер пустой
        @return  true - если контейнер пустой, false - иначе
     */
    bool isEmpty() const noexcept;

    /*!
        Возвращает обшее количество параметров в контейнере
        @return количество параметров
    */
    qsizetype count() const noexcept;

private:
    //Удаляем неиспользуемые конструторы
    Q_DISABLE_COPY_MOVE(ButtonData);

private:
    QHash<QString, QString> _params; ///< список параметров

};
