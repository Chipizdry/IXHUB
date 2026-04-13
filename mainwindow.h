#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQuickWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Публичные методы для обновления значений
    void updateSpeed(int value);
    void updatePower(int value);
    void updateTorque(int value);

public slots:
    void onSpeedChanged(int value) { updateSpeed(value); }
    void onPowerChanged(int value) { updatePower(value); }
    void onTorqueChanged(int value) { updateTorque(value); }
    void onPowerButtonToggled(bool state);

private:
    Ui::MainWindow *ui;

    // Указатели на виджеты
    QQuickWidget *speedWidget = nullptr;
    QQuickWidget *powerWidget = nullptr;
    QQuickWidget *torqueWidget = nullptr;
    QQuickWidget *powerButtonWidget;
};

#endif
