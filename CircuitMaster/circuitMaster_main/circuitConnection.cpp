#include "circuitConnection.h"

CircuitConnection::CircuitConnection()
{

}

CircuitConnection::CircuitConnection(ConnectionType startType)
{
    this->type = startType;
}

CircuitConnection::CircuitConnection(ConnectionType startType, CircuitElement startElem)
{
    this->type = startType;
    this->addElement(startElem);
}

CircuitConnection::CircuitConnection(ConnectionType startType, std::complex<double> startResistance)
{
    this->type = startType;
    this->resistance = startResistance;
}

void CircuitConnection::setVoltage(std::complex<double> newVolt)
{
    this->voltage = newVolt;
    isVoltageSet = true;
}

void CircuitConnection::setCurrent(std::complex<double> newCurr)
{
    this->current = newCurr;
    isCurrentSet = true;
}

std::complex<double> CircuitConnection::calculateResistance()
{
    // Считаем сопротивление равным нулю
    this->resistance = 0;

    // Для простого последовательного соединения
    if (this->type == ConnectionType::sequential)
    {
        // Сопротивление цепи равно сумме сопротивлений её элементов
        for (int i = 0; i < this->elements.count(); i++)
        {
            this->resistance += this->elements[i].calculateElemResistance();
        }
    }
    // Для сложного последовательного соединения
    else if(this->type == ConnectionType::sequentialComplex)
    {
        // Сопротивление цепи равно сумме сопротивлений её соединений-детей
        for (int i = 0; i < this->children.count(); i++)
        {
            this->resistance += this->children[i]->calculateResistance();
        }
    }
    // Для параллельного соединения
    else if (this->type == ConnectionType::parallel)
    {
        // Находим сумму обратных значений сопротивления соединений-детей
        std::complex<double> reverseSum = 0;
        for (int i = 0; i < this->children.count(); i++)
        {
            reverseSum += 1.0 / this->children[i]->calculateResistance();
        }
        // Находим сопротивление параллельной цепи
        this->resistance = 1.0 / reverseSum;
    }
    return this->resistance;
}

void CircuitConnection::calculateCurrentAndVoltage()
{
    // Если есть соединение-родитель - "наследуем" значения тока или напряжения
    if (this->parent != NULL)
    {
        // Наследуем силу тока если родитель - последовательное соединение
        if (this->parent->type == CircuitConnection::ConnectionType::sequentialComplex)
            //this->current = parent->current;
            this->setCurrent(parent->current);
        // Наследуем напряжение если родитель - параллельное соединение
        else if (this->parent->type == CircuitConnection::ConnectionType::parallel)
            //this->voltage = parent->voltage;
            this->setVoltage(this->parent->voltage);
    }

    if (!this->isCurrentSet && !this->isVoltageSet)
    {
        throw QString("Unknown voltage and current without parent element");
    }

    // Вычисляем оставшуюся неизвестную величину
    if (!this->isCurrentSet)
        this->current = this->voltage / this->resistance;
    else if (!this->isVoltageSet)
        this->voltage = this->current * this->resistance;


    // Рекурсивно вычисляем силу тока и напряжение всех детей, если они имеются
    bool hasChildren = this->children.count() != 0;
    if (hasChildren)
    {
        for(int i = 0; i < this->children.count(); i++)
        {
            CircuitConnection* child = this->children[i];
            child->calculateCurrentAndVoltage();
        }
    }

}

void CircuitConnection::addElement(CircuitElement newElem)
{
    this->elements.append(newElem);
}

void CircuitConnection::addChild(CircuitConnection* newChildCircuit)
{
    this->children.append(newChildCircuit);
    newChildCircuit->parent = this;
}

CircuitConnection::ConnectionType CircuitConnection::strToConnectionType(QString strType)
{
    CircuitConnection::ConnectionType type;
    if (strType == "seq")
        type = CircuitConnection::ConnectionType::sequential;
    else if (strType == "par")
        type = CircuitConnection::ConnectionType::parallel;
    else
        type = CircuitConnection::ConnectionType::invalid;
    return type;
}

CircuitConnection* CircuitConnection::connectionFromDocElement(QMap<QString, CircuitConnection>& map, QDomNode node)
{
    QDomElement element = node.toElement();
    QString nodeType = element.tagName();
    QDomNodeList children = node.childNodes();

    // Создаём новый объект соединения
    CircuitConnection newCircuit;

    // Присваеваем ему уникальный id
    int newId = map.keys().count() + 1;
    newCircuit.id = newId;

    // Получаем название соединения
    QString newName = element.attribute("name", "");
    if (newName == "")
    {
        newName = QString::number(newId);
        newCircuit.hasCustomName = false;
    }
    else
    {
        newCircuit.hasCustomName = true;
    }
    qDebug() << newName;
    newCircuit.name = newName;

    // Проверяем имя на уникальность
    QList<QString> usedNames = map.keys();
    for (int i = 0; i < usedNames.count(); i++)
    {
        if (usedNames[i] == newName)
            throw QString("Имя соединения должно быть уникальным. Повтор имени соединения на строке %1.").arg(QString::number(element.lineNumber()));
    }

    // Получаем значение напряжения, если указано
    double voltageAtr = element.attribute("voltage", "-1").toDouble();
     if (voltageAtr != -1)
         newCircuit.setVoltage(voltageAtr);

    // Добавляем объект в QMap всех соединений цепи
    map.insert(newName, newCircuit);
    qDebug() << "Added " << element.tagName();

    // Указатель на новый объект соединения в QMap для дальнейшего его заполнения
    CircuitConnection* circuit = &map[newName];

    // Определить тип соединения
    CircuitConnection::ConnectionType circuitType = CircuitConnection::strToConnectionType(nodeType);
    bool hasSeqInside = !node.firstChildElement("seq").isNull();
    bool hasParInside = !node.firstChildElement("par").isNull();
    if (circuitType == CircuitConnection::ConnectionType::sequential && (hasSeqInside || hasParInside))
        circuitType = CircuitConnection::ConnectionType::sequentialComplex;

    circuit->type = circuitType;

    // Для сложного последовательного соединения
    if (circuitType == CircuitConnection::ConnectionType::sequentialComplex || circuitType == CircuitConnection::ConnectionType::parallel)
    {
        // Рекурсивно обрабатываем каждого ребёнка текущей цепи
        for(int i = 0; i < children.count(); i++)
        {
            CircuitConnection* currChildPtr = connectionFromDocElement(map, children.at(i));
            if (currChildPtr != NULL)
            {
                circuit->addChild(currChildPtr);
            }
        }
    }
    // Для простого последовательного соединения
    else if (circuitType == CircuitConnection::ConnectionType::sequential)
    {
        if (children.isEmpty())
        {
            throw QString("Отсутсвуют элементы соединения на строке %1").arg(QString::number(element.lineNumber()));
        }
        // Добавляем все элементы в соединение
        for(int i = 0; i < children.count(); i++)
        {
            QDomElement currElem = children.at(i).toElement();
            if (currElem.tagName() == "elem")
            {
                QDomElement elem = currElem.firstChildElement("type");
                if (elem.isNull())
                {
                    throw QString("Отсутсвует тип элемента на строке %1").arg(QString::number(currElem.lineNumber()));
                }
                CircuitElement::ElemType type = CircuitElement::elemTypeFromStr(elem.text());
                if (type == CircuitElement::ElemType::invalid)
                {
                    throw QString("Неверный тип элемента на строке %1").arg(QString::number(currElem.lineNumber()));
                }
                double value = currElem.firstChildElement("value").text().toDouble();
                circuit->addElement(CircuitElement(type, value));
            }
        }
    }
    return circuit;
}
