#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTemporaryFile>
#include <QStandardItemModel>
#include <curl/curl.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDownloadClicked();
    void onBackClicked();
    void onItemDoubleClicked(const QModelIndex &index);

private:
    QString normalizePath(QString path);
    Ui::MainWindow *ui;
    QStandardItemModel *dirModel;
    CURL *curl;
    QString currentPath;
    QString baseUrl;

    static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
    static size_t writeFileCallback(void *ptr, size_t size, size_t nmemb, FILE *stream);
    void listDirectory(const QString &path);
    void parseFtpListing(const QString &data);
    void downloadFile(const QString &fileName);
    void changeDirectory(const QString &dirName);
    void showError(const QString &message);
    void updateProgress(qint64 bytesReceived, qint64 bytesTotal);
};
#endif
