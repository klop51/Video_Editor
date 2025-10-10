#include "ui/widget_inspector.hpp"
#include <QApplication>
#include <QWidget>
#include <QMetaObject>
#include <QMetaProperty>
#include <QVariant>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QTimer>
#include <algorithm>

namespace ve::ui {

WidgetInspector* WidgetInspector::s_instance = nullptr;

WidgetInspector* WidgetInspector::instance() {
    if (!s_instance) {
        s_instance = new WidgetInspector();
    }
    return s_instance;
}

void WidgetInspector::destroyInstance() {
    delete s_instance;
    s_instance = nullptr;
}

WidgetInspector::WidgetInspector(QObject* parent)
    : QObject(parent) {
}

WidgetInspector::~WidgetInspector() {
}

QList<QWidget*> WidgetInspector::findWidgets(const QString& selector, QWidget* root) {
    if (!root) {
        // Start from all top-level widgets
        QList<QWidget*> results;
        const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget* topLevel : topLevelWidgets) {
            results.append(findWidgets(selector, topLevel));
        }
        return results;
    }

    QList<QWidget*> results;

    // Parse selector
    if (selector.startsWith("#")) {
        // ID selector: #widgetName
        QString name = selector.mid(1);
        results = findByName(name, root);
    } else if (selector.contains("[")) {
        // Property selector: Type[property=value]
        results = findByProperty(selector, root);
    } else {
        // Type selector: QPushButton
        results = findByType(selector, root);
    }

    return results;
}

QWidget* WidgetInspector::findWidget(const QString& selector, QWidget* root) {
    QList<QWidget*> widgets = findWidgets(selector, root);
    return widgets.isEmpty() ? nullptr : widgets.first();
}

QVariant WidgetInspector::getProperty(QWidget* widget, const QString& propertyName) const {
    if (!widget) return QVariant();

    // Try Qt property system first
    const QMetaObject* metaObject = widget->metaObject();
    int propertyIndex = metaObject->indexOfProperty(propertyName.toUtf8().constData());
    if (propertyIndex >= 0) {
        QMetaProperty property = metaObject->property(propertyIndex);
        if (property.isReadable()) {
            return property.read(widget);
        }
    }

    // Fallback to dynamic properties
    return widget->property(propertyName.toUtf8().constData());
}

bool WidgetInspector::setProperty(QWidget* widget, const QString& propertyName, const QVariant& value) {
    if (!widget) return false;

    // Try Qt property system first
    const QMetaObject* metaObject = widget->metaObject();
    int propertyIndex = metaObject->indexOfProperty(propertyName.toUtf8().constData());
    if (propertyIndex >= 0) {
        QMetaProperty property = metaObject->property(propertyIndex);
        if (property.isWritable()) {
            return property.write(widget, value);
        }
    }

    // Fallback to dynamic properties
    widget->setProperty(propertyName.toUtf8().constData(), value);
    return true;
}

std::map<std::string, std::string> WidgetInspector::getProperties(QWidget* widget) const {
    std::map<std::string, std::string> properties;

    if (!widget) return properties;

    const QMetaObject* metaObject = widget->metaObject();
    int propertyCount = metaObject->propertyCount();

    for (int i = 0; i < propertyCount; ++i) {
        QMetaProperty property = metaObject->property(i);
        const char* name = property.name();
        QVariant value = property.read(widget);
        properties[name] = value.toString().toStdString();
    }

    // Add dynamic properties
    const QList<QByteArray> dynamicProperties = widget->dynamicPropertyNames();
    for (const QByteArray& name : dynamicProperties) {
        QVariant value = widget->property(name.constData());
        properties[name.toStdString()] = value.toString().toStdString();
    }

    return properties;
}

QString WidgetInspector::getWidgetInfo(QWidget* widget) const {
    if (!widget) return "null";

    QString info = QString("Widget {\n");
    info += QString("  type: %1\n").arg(widget->metaObject()->className());
    info += QString("  name: '%1'\n").arg(widget->objectName());
    info += QString("  geometry: %1x%2 @ (%3,%4)\n").arg(widget->width()).arg(widget->height()).arg(widget->x()).arg(widget->y());
    info += QString("  visible: %1\n").arg(widget->isVisible() ? "true" : "false");
    info += QString("  enabled: %1\n").arg(widget->isEnabled() ? "true" : "false");

    // Add some common properties
    if (widget->inherits("QLabel")) {
        info += QString("  text: '%1'\n").arg(getProperty(widget, "text").toString());
    } else if (widget->inherits("QPushButton")) {
        info += QString("  text: '%1'\n").arg(getProperty(widget, "text").toString());
        info += QString("  checked: %1\n").arg(getProperty(widget, "checked").toBool() ? "true" : "false");
    }

    info += "}";
    return info;
}

QString WidgetInspector::getWidgetTree(QWidget* root, int maxDepth) const {
    QString output;
    if (!root) {
        const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        for (QWidget* topLevel : topLevelWidgets) {
            if (topLevel->isVisible()) {
                buildWidgetTree(output, topLevel, 0, maxDepth);
            }
        }
    } else {
        buildWidgetTree(output, root, 0, maxDepth);
    }
    return output;
}

QList<QWidget*> WidgetInspector::getChildren(QWidget* parent) const {
    if (!parent) return QList<QWidget*>();

    QList<QWidget*> children;
    const QObjectList childObjects = parent->children();
    for (QObject* child : childObjects) {
        if (QWidget* widget = qobject_cast<QWidget*>(child)) {
            children.append(widget);
        }
    }
    return children;
}

QWidget* WidgetInspector::getParent(QWidget* child) const {
    return child ? qobject_cast<QWidget*>(child->parent()) : nullptr;
}

void WidgetInspector::simulateClick(QWidget* widget) {
    if (!widget || !widget->isVisible() || !widget->isEnabled()) return;

    // Create mouse press and release events at widget center
    QPoint center = widget->rect().center();
    QMouseEvent pressEvent(QEvent::MouseButtonPress, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    // Send events
    QCoreApplication::sendEvent(widget, &pressEvent);
    QCoreApplication::sendEvent(widget, &releaseEvent);

    if (m_debugLogging) {
        qDebug() << "Simulated click on" << widget->objectName() << "(" << widget->metaObject()->className() << ")";
    }
}

void WidgetInspector::simulateKeyPress(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers) {
    if (!widget) return;

    QKeyEvent pressEvent(QEvent::KeyPress, key, modifiers);
    QKeyEvent releaseEvent(QEvent::KeyRelease, key, modifiers);

    QCoreApplication::sendEvent(widget, &pressEvent);
    QCoreApplication::sendEvent(widget, &releaseEvent);

    if (m_debugLogging) {
        qDebug() << "Simulated key press" << key << "on" << widget->objectName();
    }
}

void WidgetInspector::enableDebugLogging(bool enabled) {
    m_debugLogging = enabled;
}

void WidgetInspector::dumpWidgetTree(QWidget* root) const {
    QString tree = getWidgetTree(root);
    qDebug().noquote() << "Widget Tree:" << tree;
}

// Private implementation

QList<QWidget*> WidgetInspector::findByType(const QString& typeName, QWidget* root) const {
    QList<QWidget*> results;

    if (root->inherits(typeName.toUtf8().constData())) {
        results.append(root);
    }

    // Recursively search children
    const QList<QWidget*> children = getChildren(root);
    for (QWidget* child : children) {
        results.append(findByType(typeName, child));
    }

    return results;
}

QList<QWidget*> WidgetInspector::findByName(const QString& name, QWidget* root) const {
    QList<QWidget*> results;

    if (root->objectName() == name) {
        results.append(root);
    }

    // Recursively search children
    const QList<QWidget*> children = getChildren(root);
    for (QWidget* child : children) {
        results.append(findByName(name, child));
    }

    return results;
}

QList<QWidget*> WidgetInspector::findByProperty(const QString& propertySpec, QWidget* root) const {
    // Parse selector like "QPushButton[text=Play]"
    QRegularExpression regex(R"((\w+)\[(\w+)=(.+)\])");
    QRegularExpressionMatch match = regex.match(propertySpec);

    if (!match.hasMatch()) {
        return QList<QWidget*>();
    }

    QString typeName = match.captured(1);
    QString propertyName = match.captured(2);
    QString propertyValue = match.captured(3);

    // Remove quotes if present
    if (propertyValue.startsWith("'") && propertyValue.endsWith("'")) {
        propertyValue = propertyValue.mid(1, propertyValue.length() - 2);
    }

    QList<QWidget*> results;

    // Check if this widget matches
    if (root->inherits(typeName.toUtf8().constData())) {
        QVariant value = getProperty(root, propertyName);
        if (value.toString() == propertyValue) {
            results.append(root);
        }
    }

    // Recursively search children
    const QList<QWidget*> children = getChildren(root);
    for (QWidget* child : children) {
        results.append(findByProperty(propertySpec, child));
    }

    return results;
}

void WidgetInspector::buildWidgetTree(QString& output, QWidget* widget, int currentDepth, int maxDepth, const QString& indent) const {
    if (!widget || currentDepth > maxDepth) return;

    QString typeName = widget->metaObject()->className();
    QString name = widget->objectName();
    QString visibility = widget->isVisible() ? "visible" : "hidden";

    output += QString("%1%2 (%3) [%4]\n").arg(indent).arg(typeName).arg(name).arg(visibility);

    if (currentDepth < maxDepth) {
        QString childIndent = indent + "  ";
        const QList<QWidget*> children = getChildren(widget);
        for (QWidget* child : children) {
            buildWidgetTree(output, child, currentDepth + 1, maxDepth, childIndent);
        }
    }
}

} // namespace ve::ui