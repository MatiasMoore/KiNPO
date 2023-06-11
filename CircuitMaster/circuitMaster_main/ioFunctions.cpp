#include "ioFunctions.h"

/*!
*\file
*\brief Реализация функций для работы с файлами
*/

QString complexToStr(std::complex<double> num)
{
    QString str;
    double real = num.real();
    double imag = num.imag();
    if (imag == 0)
        str = QString::number(real);
    else
    {
        QString sign = imag > 0 ? "+" : "-";
        str = QString("%1 %2 %3i").arg(QString::number(real), sign, QString::number(abs(imag)));
    }
    return str;
}

void readInputFromFile(QString inputPath, QMap<QString, CircuitConnection>& circuitMap)
{
    // Создаем QFile на основе пути
    QFile xmlFile(inputPath);

    // Попытатья открыть файл
    // Ошибка, если не удалось открыть
    if (!xmlFile.exists() || !xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        throw QString("Неверно указан файл для входных данных. Возможно указанного расположения не существует или нет прав на запись.");
    }

    // Создаем QDomDocument
    QDomDocument domDocument;

    // Переменные для получения ошибки от QDomDoc
    QString errorMes;
    int errorLine;

    // Пытаемся создать QDomDoc
    // Ошибка, если не удалось создать QDomDoc
    if (!domDocument.setContent(&xmlFile, &errorMes, &errorLine))
    {
        throw QString("%1 at line %2").arg(errorMes, QString::number(errorLine));
    }

    // Закрываем файл, по завершении работы
    xmlFile.close();

    // Получаем корневой элемент QDomDoc
    QDomElement rootElement = domDocument.documentElement();

    // Обработка ошибок корневого элемента
    QString rootTag = rootElement.tagName();
    if (rootTag != "seq" && rootTag != "par")
        throw QString("Корневым элементом должно быть последовательное \"<seq>\" или параллельное \"<par>\" соединение.");

    QString voltageStr = rootElement.attribute("voltage", "");
    if (voltageStr.length() == 0)
        throw QString("У корневого элемента должно быть указано напряжение.");

    QString frequencyStr = rootElement.attribute("frequency", "");
    // Значение -1 означает, что частота неизвестна
    double frequency = -1;
    if (frequencyStr.length() > 0)
    {
        // Пытаемся конвертировать в double
        bool convertedOk;
        frequency = frequencyStr.toDouble(&convertedOk);

        // Ошибка, если не удалость конверертировать
        if (!convertedOk)
            throw QString("Неверный формат значения частоты у корневого элемента.");
    }

    // Элементы всех соединений цепи, кроме корневого
    QDomNodeList seqConnections = rootElement.elementsByTagName("seq");
    QDomNodeList parConnections = rootElement.elementsByTagName("par");
    auto conns = { seqConnections, parConnections};

    // Обработка ошибок связанных с указанием напряжения или частоты у других соединений
    for (auto connGroup = conns.begin(); connGroup != conns.end(); connGroup++)
    {
        // Для каждого соединения
        for (int i = 0; i < connGroup->count(); i++)
        {
            QDomElement connectionElement = connGroup->at(i).toElement();

            // Ошибка, если указано напряжение
            if (connectionElement.attribute("voltage", "").length() != 0)
                throw QString("Неверное указание напряжения цепи на строке %1. "
                              "Напряжение указывается только для корневого элемента схемы").arg(QString::number(connectionElement.lineNumber()));

            // Ошибка, если указана частота
            if (connectionElement.attribute("frequency", "").length() != 0)
                throw QString("Неверное указание частоты переменного тока на строке %1. "
                              "Частота указывается только для корневого элемента схемы").arg(QString::number(connectionElement.lineNumber()));
        }
    }

    // Создаем дерево соединений в QMap
    CircuitConnection::connectionFromDocElement(circuitMap, rootElement, frequency);
}

void writeOutputToFile(QString outputPath, QMap<QString, CircuitConnection>& circuitMap)
{
    QFile outFile(outputPath);
    if (!outFile.open(QFile::WriteOnly | QFile::Text)) {
        throw QString("Неверно указан файл для выходных данных. Возможно указанного расположения не существует или нет прав на запись.");
    }

    auto keyIter = circuitMap.keyBegin();

    while (keyIter != circuitMap.keyEnd())
    {
        CircuitConnection& currCirc = circuitMap[*keyIter];
        if (currCirc.hasCustomName)
        {
            QString outLine = QString("%1 = %2\n").arg(currCirc.name, complexToStr(currCirc.current));
            outFile.write(outLine.toStdString().c_str());
        }
        keyIter++;
    }

    outFile.close();
}
