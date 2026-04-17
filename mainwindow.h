



#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQuickWidget>
#include <QTimer>

class RelayDevice;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct ChartData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPowerButtonToggled(bool state);
    void updateIndicators();
    void updateChart();
    void initializeChart();

    // Слоты для последовательности включения
    void onStartupTimerTimeout();
    void checkRelayStatus();
    void onRelayStateChanged(const QString& relayName, bool state);

    // НОВЫЙ СЛОТ ДЛЯ ОТПРАВКИ КОМАНД
    void onRelayCommandGenerated(const QByteArray& command);

private:
    void updateSpeed(int value);
    void updatePower(int value);
    void updateTorque(int value);

    // Вспомогательные методы
    void setButtonBlinking(bool blinking, bool finalState = false);
    void sendRelayCommand(const QString& relayName, bool state);
    void startRelaySequence();

    Ui::MainWindow *ui;

    // Виджеты
    QQuickWidget *powerWidget;
    QQuickWidget *speedWidget;
    QQuickWidget *torqueWidget;
    QQuickWidget *powerButtonWidget;
    QQuickWidget *m_chartWidget;

    // Устройства
    RelayDevice *m_relayDevice;

    // Данные для графика
    ChartData *m_chartData;

    // Таймеры
    QTimer *m_startupTimer;
    QTimer *m_statusCheckTimer;

    // Состояния
    bool m_isStarting;
    int m_currentStep;
};

#endif // MAINWINDOW_H



