
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QQuickWidget>
#include <QUrl>
#include <QTimer>
#include <QTabBar>
#include <QQuickItem>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *infoTab = ui->Info;
    delete infoTab->layout();

    QVBoxLayout *layout = new QVBoxLayout(infoTab);

    QQuickWidget *quickWidget = new QQuickWidget(this);
    quickWidget->setMinimumSize(400, 400);
    quickWidget->setSource(QUrl("qrc:/qml/Speedometer.qml"));

   // layout->addWidget(quickWidget);
     quickWidget->move(300, 150);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [quickWidget]() {
        static int speed = 0;
        speed += 50;
        if (speed > 10000) speed = 0;

        if (quickWidget->rootObject()) {
            quickWidget->rootObject()->setProperty("speedValue", speed);
        }
    });
    timer->start(100);  // Обновление каждые 100 мс



    ui->tabWidget->tabBar()->setExpanding(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}
