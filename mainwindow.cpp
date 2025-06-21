#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QDir>
#include <cstdio>
#include <QTimer>
#include <QUrl>
#include <unistd.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), curl(curl_easy_init()),
    dirModel(new QStandardItemModel(this))
{
    curl_version_info_data *vdata = curl_version_info(CURLVERSION_NOW);
    if (vdata) {
        qDebug() << "Using libcurl version:" << vdata->version;
    }
    ui->setupUi(this);
    ui->listView->setModel(dirModel);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);

    if (!curl) {
        showError("Не удалось инициализировать CURL");
        QTimer::singleShot(0, this, &QApplication::quit);
        return;
    }

    connect(ui->pushButton_connect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->pushButton_download, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    connect(ui->pushButton_back, &QPushButton::clicked, this, &MainWindow::onBackClicked);
    connect(ui->listView, &QListView::doubleClicked, this, &MainWindow::onItemDoubleClicked);

    ui->progressBar->setValue(0);
    ui->label_path->setText("Текущий путь: /");
}

MainWindow::~MainWindow()
{
    if (curl) {
        curl_easy_cleanup(curl);
    }
    delete ui;
}


size_t MainWindow::writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    MainWindow *self = static_cast<MainWindow*>(userp);
    QByteArray data((char*)contents, size * nmemb);
    self->parseFtpListing(QString::fromUtf8(data));
    return size * nmemb;
}

size_t MainWindow::writeFileCallback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

void MainWindow::onConnectClicked()
{
    QString url = ui->lineEdit->text().trimmed();
    if (url.isEmpty()) {
        showError("Введите адрес FTP сервера");
        return;
    }

    if (url.startsWith("ftp://")) {
        baseUrl = url;
    } else {
        baseUrl = "ftp://" + url;
    }

    currentPath = "/";
    listDirectory(currentPath);
}

QString MainWindow::normalizePath(QString path)
{
    // Удаляем все дублирующиеся слеши
    path.replace("//", "/");

    // Убедимся, что путь начинается с /
    if(!path.startsWith('/')) {
        path.prepend('/');
    }

    // Удаляем завершающий слеш, если это не корень
    if(path.length() > 1 && path.endsWith('/')) {
        path.chop(1);
    }

    return path;
}

void MainWindow::listDirectory(const QString &path)
{
    if (!curl) return;

    dirModel->clear();
    QString fullUrl = baseUrl;

    // Добавляем путь к базовому URL
    if(!path.isEmpty() && path != "/") {
        if(!fullUrl.endsWith('/') && !path.startsWith('/')) {
            fullUrl += '/';
        }
        fullUrl += path;
    }

    // Для корневой директории добавляем завершающий слеш
    if(fullUrl.endsWith("ftp:/")) {
        fullUrl += '/';
    }
    else if(!fullUrl.endsWith('/')) {
        fullUrl += '/';
    }

    qDebug() << "Requesting URL:" << fullUrl;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "anonymous:user@example.com");
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 0L);
    curl_easy_setopt(curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L);

    ui->statusbar->showMessage("Загрузка содержимого...");
    qApp->processEvents();

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        showError(QString("Ошибка CURL: %1").arg(curl_easy_strerror(res)));
    } else {
        ui->label_path->setText("Текущий путь: " + path);
        ui->statusbar->showMessage("Каталог успешно загружен");
    }
}


void MainWindow::parseFtpListing(const QString &data)
{
    if (data.isEmpty()) {
        showError("Сервер вернул пустой ответ");
        return;
    }

    QStringList lines = data.split('\n', Qt::SkipEmptyParts);

    foreach(const QString &line, lines) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if(parts.size() < 9) continue;

        bool isDir = parts[0].startsWith('d');
        QString name = parts.mid(8).join(' ').trimmed();
        if(name.isEmpty() || name == "." || name == "..") continue;

        QStandardItem *item = new QStandardItem(name);
        item->setIcon(isDir ? QIcon::fromTheme("folder") : QIcon::fromTheme("text-x-generic"));
        item->setData(isDir, Qt::UserRole);
        dirModel->appendRow(item);
    }
}

void MainWindow::onItemDoubleClicked(const QModelIndex &index)
{
    if(!index.isValid()) return;

    QStandardItem *item = dirModel->itemFromIndex(index);
    if(!item) return;

    // Проверяем, что это действительно директория
    if(item->data(Qt::UserRole).toBool()) {
        changeDirectory(item->text());
    } else {
        ui->statusbar->showMessage("Для скачивания файла используйте кнопку 'Скачать'");
    }
}

void MainWindow::changeDirectory(const QString &dirName)
{
    if(baseUrl.isEmpty()) {
        showError("Сначала подключитесь к серверу");
        return;
    }

    QString newPath = currentPath;

    if(dirName == "..") {
        // Поднимаемся на уровень выше
        int lastSlash = newPath.lastIndexOf('/');
        if(lastSlash > 0) {
            newPath = newPath.left(lastSlash);
        } else {
            newPath = "/";
        }
    } else {
        // Добавляем новую директорию к пути
        if(!newPath.endsWith('/')) {
            newPath += '/';
        }
        newPath += QUrl::toPercentEncoding(dirName);
    }

    // Нормализуем путь
    newPath = normalizePath(newPath);

    // Проверяем, не вышли ли мы за пределы корня
    if(newPath.isEmpty()) {
        newPath = "/";
    }

    currentPath = newPath;
    listDirectory(currentPath);
}

void MainWindow::onBackClicked()
{
    changeDirectory("..");
}

void MainWindow::onDownloadClicked()
{
    QModelIndex index = ui->listView->currentIndex();
    if(!index.isValid()) {
        showError("Выберите файл для скачивания");
        return;
    }

    QStandardItem *item = dirModel->itemFromIndex(index);
    if(item->data(Qt::UserRole).toBool()) {
        showError("Нельзя скачать директорию");
        return;
    }

    downloadFile(item->text());
}
void MainWindow::downloadFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        showError("Не указано имя файла");
        return;
    }

    // 1. Формирование URL
    QString fileUrl = baseUrl;
    if (!currentPath.startsWith('/')) fileUrl += '/';
    fileUrl += currentPath;
    if (!fileUrl.endsWith('/')) fileUrl += '/';
    fileUrl += QUrl::toPercentEncoding(fileName);
    fileUrl = fileUrl.replace("//", "/").replace(":/", "://");

    qDebug() << "Downloading file from:" << fileUrl;

    // 2. Выбор места сохранения
    QString savePath = QFileDialog::getSaveFileName(this,
                                                    "Сохранить файл",
                                                    QDir::homePath() + "/" + fileName,
                                                    "All Files (*)");

    if (savePath.isEmpty()) {
        qDebug() << "Download cancelled by user";
        return;
    }

    // 3. Создание временного файла
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        showError("Ошибка создания временного файла: " + tempFile.errorString());
        return;
    }
    QString tempFilePath = tempFile.fileName();
    tempFile.close();

    // 4. Настройка CURL для загрузки
    FILE *file = fopen(tempFilePath.toUtf8().constData(), "wb");
    if (!file) {
        showError("Не удалось открыть файл для записи");
        return;
    }

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, fileUrl.toUtf8().constData());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "anonymous:user@example.com");
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // Исправленная часть с callback для прогресса
    auto progressCallback = [](void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) -> int {
        MainWindow *self = static_cast<MainWindow*>(clientp);
        QMetaObject::invokeMethod(self, "updateProgress",
                                  Qt::QueuedConnection,
                                  Q_ARG(qint64, dlnow),
                                  Q_ARG(qint64, dltotal));
        return 0;
    };

    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

    // 5. Выполнение загрузки
    ui->statusbar->showMessage("Скачивание " + fileName + "...");
    ui->progressBar->setValue(0);
    qApp->processEvents();

    CURLcode res = curl_easy_perform(curl);
    fclose(file);

    // 6. Обработка результата
    if (res != CURLE_OK) {
        showError(QString("Ошибка загрузки: %1").arg(curl_easy_strerror(res)));
        QFile::remove(tempFilePath);
        return;
    }

    // 7. Перенос файла в конечное местоположение
    if (QFile::exists(savePath)) {
        QFile::remove(savePath);
    }

    if (!QFile::rename(tempFilePath, savePath)) {
        showError("Не удалось сохранить файл в указанное место");
        QFile::remove(tempFilePath);
        return;
    }

    ui->statusbar->showMessage("Файл успешно скачан");
    ui->progressBar->setValue(100);
    qDebug() << "File successfully downloaded to:" << savePath;
}

void MainWindow::updateProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        ui->progressBar->setMaximum(static_cast<int>(bytesTotal));
        ui->progressBar->setValue(static_cast<int>(bytesReceived));
    }
}
void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "Ошибка", message);
    ui->statusbar->showMessage(message);
}
