#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "listwidgetpeeritem.h"
#include "listwidgetlogitem.h"
#include "oslib.h"

#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QUrl>
#include <QTime>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass), mProtocol(NULL)
{
    ui->setupUi(this);
    ui->statusBar->showMessage("Drag a file in this window to send it.");
    ui->labelOutput->setText(OsLib::adaptPath(QDir::currentPath()));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setProtocolReference(DuktoProtocol *p)
{
    mProtocol = p;
    connect(p, SIGNAL(peerListChanged()), this, SLOT(refreshPeerList()));
    connect(p, SIGNAL(sendFileComplete(QString)), this, SLOT(sendFileComplete(QString)));
    connect(p, SIGNAL(sendFileError(int)), this, SLOT(sendFileError(int)));
    connect(p, SIGNAL(receiveFileStart()), this, SLOT(receiveFileStart()));
    connect(p, SIGNAL(receiveFileComplete(QString)), this, SLOT(receiveFileComplete(QString)));
    connect(p, SIGNAL(receiveFileCancelled(QString)), this, SLOT(receiveFileCancelled(QString)));
    connect(p, SIGNAL(transferStatusUpdate(int)), this, SLOT(transferStatusUpdate(int)), Qt::DirectConnection);
}

// Aggiornamento lista dei client
void MainWindow::refreshPeerList()
{
    QHash<QString, Peer>& peers = mProtocol->getPeers();
    QStringList list;
    QHashIterator<QString, Peer> i(peers);

    // Salvataggio elemento corrente
    QString current;
    if (ui->listPeers->selectedItems().count() > 0)
    {
        ListWidgetPeerItem *i = static_cast<ListWidgetPeerItem*>(ui->listPeers->selectedItems().at(0));
        current = i->getPeerKey();
    }

    // Cancellazione lista corrente
    ui->listPeers->clear();

    // Riempimento lista
    while (i.hasNext())
    {
        i.next();
        new ListWidgetPeerItem(ui->listPeers, i.value().name, i.key());
        if (i.key() == current)
            ui->listPeers->setCurrentRow(ui->listPeers->count() - 1);
    }

    // Selezione primo elemento
    if ((ui->listPeers->count() > 0) && (ui->listPeers->selectedItems().count() == 0))
        ui->listPeers->setCurrentRow(0);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasFormat("text/uri-list"))
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QList<QUrl> urlList;
    QStringList files;

    if (e->mimeData()->hasUrls())
    {
        urlList = e->mimeData()->urls();
        for (int i = 0; i < urlList.size(); i++)
            files.append(OsLib::removeUrlPrefix(urlList[i].toString()));
        startFileTransfer(files);
    }

    e->acceptProposedAction();
}

void MainWindow::startFileTransfer(QStringList files)
{

    QString dest;
    if (ui->tabWidget->currentIndex() == 0)
    {
        if (ui->listPeers->selectedItems().count() > 0) {
            ListWidgetPeerItem *i = static_cast<ListWidgetPeerItem*>(ui->listPeers->selectedItems().at(0));
            dest = i->getPeerKey();
        }
        else {
            QMessageBox::critical(this, "Error", "No peer available as target.");
            return;
        }

    }
    else if (ui->tabWidget->currentIndex() == 1)
    {
        dest = ui->textDestination->text();
        if (dest.length() == 0) {
            QMessageBox::critical(this, "Error", "No ip address or hostname specified.");
            return;
        }
    }
    else
    {
        QMessageBox::critical(this, "Error", "Before dropping a file here, switch to the 'available clients' or to the 'manual IP connection' modes.");
        return;
    }

    ui->tabWidget->setEnabled(false);
    ui->statusBar->showMessage("Sending file...");
    ui->progressBar->setValue(0);
    mProtocol->sendFile(dest, files);
}

void MainWindow::sendFileComplete(QString name)
{
    ui->statusBar->showMessage("File '" + name + "' sent.");
    log("File '" + name + "' sent.", name);
    ui->tabWidget->setEnabled(true);
}

void MainWindow::receiveFileStart()
{
    ui->progressBar->setValue(0);
    ui->tabWidget->setEnabled(false);
    ui->statusBar->showMessage("Receiving file...");
}

void MainWindow::receiveFileComplete(QString name)
{
    ui->tabWidget->setEnabled(true);
    ui->statusBar->showMessage("File '" + name +  "' received.");
    log("File '" + name +  "' received.", name);
    QApplication::alert(this, 3000);
}

void MainWindow::receiveFileCancelled(QString name)
{
    ui->tabWidget->setEnabled(true);
    ui->progressBar->setValue(0);
    ui->statusBar->showMessage("Transfer of file '" + name +  "' cancelled.");
    log("Transfer of file '" + name +  "' cancelled.", "");
}

void MainWindow::transferStatusUpdate(int p)
{
    ui->progressBar->setValue(p);
}

void MainWindow::on_buttonSendToIp_clicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select file to send", "", "Any file (*.*)");
    if (files.count() == 0) return;
    startFileTransfer(files);
}

void MainWindow::on_textDestination_textChanged(QString s)
{
    ui->buttonSendToIp->setEnabled((s != ""));
}

void MainWindow::sendFileError(int e)
{
    QMessageBox::critical(this, "Send file", "An error has occurred while sending the file (" + QString::number(e, 10) + ").");
    ui->progressBar->setValue(0);
    ui->tabWidget->setEnabled(true);
}

void MainWindow::on_listPeers_itemDoubleClicked(QListWidgetItem* item)
{
    QString filename = QFileDialog::getOpenFileName(this, "Select file to send", "", "Any file (*.*)");
    if (filename == "") return;
    QStringList list;
    list.append(filename);
    startFileTransfer(list);
}

void MainWindow::log(QString text, QString filename)
{
    new ListWidgetLogItem(ui->listLog, QTime::currentTime().toString("HH.mm") + ": " + text, filename);
}

void MainWindow::on_listLog_itemDoubleClicked(QListWidgetItem* item)
{
    ListWidgetLogItem *i = static_cast<ListWidgetLogItem*>(ui->listLog->selectedItems().at(0));
    QString n = QDir::currentPath() + "/" + i->getFilename();
    OsLib::openFile(n);
}

void MainWindow::on_buttonChangeDir_clicked()
{
    QString dirname = QFileDialog::getExistingDirectory(this, "Change the output folder", ".",
                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirname == "") return;
    QDir::setCurrent(dirname);
    ui->labelOutput->setText(OsLib::adaptPath(QDir::currentPath()));
}
