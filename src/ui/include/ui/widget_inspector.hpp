#pragma once

#include <QObject>
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QList>
#include <QMetaObject>
#include <QMetaProperty>
#include <QDebug>

namespace ve::ui {

/**
 * DOM-like inspector for Qt6 widgets.
 *
 * Provides programmatic access to widget hierarchy, properties, and basic interaction.
 * Useful for debugging UI state and automated testing.
 *
 * Selector syntax:
 * - "QPushButton" - Find all QPushButton widgets
 * - "#widgetName" - Find widget by object name
 * - "QLabel[text=Hello]" - Find QLabel with specific text property
 */
class WidgetInspector : public QObject {
    Q_OBJECT

public:
    static WidgetInspector* instance();
    static void destroyInstance();

    // Widget discovery and querying
    QList<QWidget*> findWidgets(const QString& selector, QWidget* root = nullptr);
    QWidget* findWidget(const QString& selector, QWidget* root = nullptr);

    // Property access
    QVariant getProperty(QWidget* widget, const QString& propertyName) const;
    bool setProperty(QWidget* widget, const QString& propertyName, const QVariant& value);
    std::map<std::string, std::string> getProperties(QWidget* widget) const;

    // Widget information
    QString getWidgetInfo(QWidget* widget) const;
    QString getWidgetTree(QWidget* root = nullptr, int maxDepth = 3) const;

    // Widget hierarchy
    QList<QWidget*> getChildren(QWidget* parent) const;
    QWidget* getParent(QWidget* child) const;

    // Event simulation (Phase 2)
    void simulateClick(QWidget* widget);
    void simulateKeyPress(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    // Debug features
    void enableDebugLogging(bool enabled);
    void dumpWidgetTree(QWidget* root = nullptr) const;

signals:
    void widgetTreeChanged();
    void widgetInspected(QWidget* widget, const QString& info);

public:
    explicit WidgetInspector(QObject* parent = nullptr);
    ~WidgetInspector() override;

    // Selector parsing
    QList<QWidget*> findByType(const QString& typeName, QWidget* root) const;
    QList<QWidget*> findByName(const QString& name, QWidget* root) const;
    QList<QWidget*> findByProperty(const QString& propertySpec, QWidget* root) const;

    // Tree traversal
    void buildWidgetTree(QString& output, QWidget* widget, int currentDepth, int maxDepth, const QString& indent = "") const;

    // Singleton instance
    static WidgetInspector* s_instance;

    bool m_debugLogging = false;
};

} // namespace ve::ui