


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQuickWidget>
#include <QTimer>
#include <QElapsedTimer>

class RelayDevice;
class BldcDriverDevice;

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
    void onRelayCommandGenerated(const QByteArray& command);

    // Слоты для BLDC драйвера
    void onBldcCommandGenerated(const QByteArray& command);
    void onBldcDataUpdated();

private:
    void updateSpeed(int value);
    void updatePower(int value);
    void updateTorque(int value);

    void setButtonBlinking(bool blinking, bool finalState = false);
    void sendRelayCommand(const QString& relayName, bool state);
    void startRelaySequence();

    Ui::MainWindow *ui;

    QQuickWidget *powerWidget;
    QQuickWidget *speedWidget;
    QQuickWidget *torqueWidget;
    QQuickWidget *powerButtonWidget;
    QQuickWidget *m_chartWidget;

    RelayDevice *m_relayDevice;
    BldcDriverDevice *m_bldcDevice;

    ChartData *m_chartData;

    QTimer *m_startupTimer;
    QTimer *m_statusCheckTimer;

    bool m_isStarting;
    int m_currentStep;
};

#endif // MAINWINDOW_H



