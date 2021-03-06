#include <QJsonDocument>
#include <QJsonObject>

#include <QStringListModel>

#include <QtGlobal>
#include <qjsonobject.h>
#include <qobject.h>

#include "ui_MainWindow.h"

#include "GRPCClient.hpp"
#include "MainWindow.h"
#include "PreferencesDialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , preferences_dialog_(nullptr)
    , config_file_(new QFile(
          QCoreApplication::applicationDirPath() + "/config.cfg", this)) {
    ui_->setupUi(this);

    QObject::connect(ui_->actionPreferences, &QAction::triggered, this,
        &MainWindow::showPreferences);

    if (!config_file_->open(QIODevice::OpenMode(QIODevice::ReadWrite))) {
        qFatal(
            "Failed to open file %s", qUtf8Printable(config_file_->fileName()));
    }
    QByteArray bytes = config_file_->readAll();
    if (bytes.isEmpty()) {
        bytes = "{}";
    }
    loadConf(QJsonDocument::fromJson(bytes));

    preferences_dialog_ = new PreferencesDialog(conf_, this);
    QObject::connect(preferences_dialog_, &PreferencesDialog::accepted, this,
        &MainWindow::saveConf);
    // QObject::connect(preferences_dialog_, &PreferencesDialog::accepted,
    // preferences_dialog_, &PreferencesDialog::delete_dialog);

    this->thread_.reset(new GRPCThreads("127.0.0.1:50051",
        "Hello_" + QString::fromStdString(std::to_string(time(NULL)))));
    this->thread_->start();
    assert(this->thread_->isRunning());

    connect(
        this->thread_.get(), &GRPCThreads::OnLogin, this, &MainWindow::onLogin);
    connect(this->thread_.get(), &GRPCThreads::OnListPeers, this,
        &MainWindow::onListPeers);

    connect(
        ui_->sendButton, &QPushButton::pressed, this, &MainWindow::sendMessage);
}

MainWindow::~MainWindow() {}

void MainWindow::showPreferences() {
    preferences_dialog_->show();
}

void MainWindow::loadConf(const QJsonDocument &doc) {
    if (!doc.isObject()) {
        qFatal("Invalid configuration");
    }
    QJsonObject o = doc.object();
    for (auto it = o.begin(); it != o.end(); ++it) {
        QString key = it.key();
        QString val = it.value().toString();
        this->conf_.emplace(key, val);
    }

    // default values
    conf_.try_emplace("Server IP", "");
    conf_.try_emplace("Server Port", "443");
    conf_.try_emplace("Your Name", "Mr. Nobody");
    conf_.try_emplace("Certificates", "");
    conf_.try_emplace("Private Key", "");
}

void MainWindow::start(QString const &host) {}

void MainWindow::stop() {}

void MainWindow::saveConf() {
    conf_ = preferences_dialog_->GetConf();
    QJsonObject o;
    for (auto const &[k, v] : conf_) {
        o[k] = v;
    }
    QJsonDocument doc;
    doc.setObject(o);
    QByteArray bytes = doc.toJson();
    config_file_->seek(0);
    config_file_->write(bytes);
}

void MainWindow::onLogin(QString const &token) {
    qDebug() << token;
    this->thread_->ListPeers();
    qDebug() << "Called ListPeers";
}

void MainWindow::sendMessage() {
    QString msg = ui_->textEdit->document()->toPlainText();
    QByteArray bytes = msg.toUtf8();
    qDebug("Send Message: %s", msg.toStdString().c_str());
    int row = ui_->peerList->currentIndex().row();
    qDebug("Current selected row %d", row);
    QAbstractItemModel *model = ui_->peerList->model();
    QStringListModel *qsl_model = dynamic_cast<QStringListModel *>(model);
    assert(qsl_model);
    Message m;
    m.peer.id = qsl_model->stringList().at(row);
    m.type = Message::MessageType::TEXT;
    m.bytes.resize(bytes.size());
    memcpy(m.bytes.data(), bytes.data(), bytes.size());
    thread_->SendMessage(m);
}

void MainWindow::onListPeers(PeerList const &list) {
    QStringList l;
    qDebug() << "ListPeers:";
    for (auto const p : list.peers) {
        qDebug() << p.id;
        l.push_back(p.id);
    }
    qDebug() << "";
    QAbstractItemModel *model = new QStringListModel(l);
    ui_->peerList->setModel(model);
}
