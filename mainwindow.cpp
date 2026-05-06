#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QRandomGenerator>
#include <QDebug>
#include <QFileInfo>
#include <QPixmap>
#include <QToolBar> // 添加 QToolBar 头文件
#include <QSizePolicy> // 添加 QSizePolicy 头文件
#include "imageviewerdialog.h"
#include <QIcon>
#include <QCoreApplication>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isRefreshing(false)
    , m_imageViewer(nullptr)
    , m_lastBackupDate()

{
    ui->setupUi(this);
    qDebug() << "MainWindow constructed";
    // 设置窗口图标
    setWindowIcon(QIcon("icon.png"));
    this->setWindowTitle("知识点记忆系统 - 麻辣兔头");
    // 初始化图片查看器
    m_imageViewer = new ImageViewerDialog(this);
    // // 设置图片标签可点击
    // ui->labelImageDisplay->setCursor(Qt::PointingHandCursor);
    // ui->labelImageDisplay->installEventFilter(this);

    // 初始化图片存储路径
    m_imageStoragePath = getImageStoragePath();
    qDebug() << "Image storage path:" << m_imageStoragePath;
    // 确保存储目录存在
    if (!ensureImageStorageDirectory()) {
        QMessageBox::warning(this, "警告", "无法创建图片存储目录，图片保存功能可能受限");
    }

    // 先清空组合框
    ui->comboStatus->clear();
    ui->comboFilterStatus->clear();

    // 初始化状态组合框
    ui->comboStatus->addItem("新知识点", STATUS_NEW);
    ui->comboStatus->addItem("学习中", STATUS_LEARNING);
    ui->comboStatus->addItem("复习中", STATUS_REVIEWING);
    ui->comboStatus->addItem("已掌握", STATUS_MASTERED);

    ui->comboFilterStatus->addItem("全部状态", "");
    ui->comboFilterStatus->addItem("新知识点", "new");
    ui->comboFilterStatus->addItem("学习中", "learning");
    ui->comboFilterStatus->addItem("复习中", "reviewing");
    ui->comboFilterStatus->addItem("已掌握", "mastered");

    // 设置图片标签
    // ui->labelImageDisplay->setMinimumSize(400, 300);
    // ui->labelImageDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // ui->labelImageDisplay->setAlignment(Qt::AlignCenter);
    // ui->labelImageDisplay->setText("图片显示区域");
    imageZoomFactor = 1.0;

    // 加载数据
    loadKnowledgePoints();
    qDebug() << "Loaded" << knowledgePoints.size() << "knowledge points";

    // 每日复习
    QTimer::singleShot(0, this, &MainWindow::startDailyReview);

    // 如果没有数据，显示提示
    if (knowledgePoints.isEmpty()) {
        qDebug() << "No knowledge points found, showing welcome message";
        ui->textContent->setPlainText("欢迎使用记忆曲线复习系统！\n请点击\"添加\"按钮创建第一个知识点。");
    }

    refreshKnowledgeList();
    updateStatistics();


    // debugDataSources();//测试可删

    // 连接信号槽
    connect(ui->btnAddNew, &QPushButton::clicked, this, &MainWindow::handleAddNew);
    connect(ui->btnEditPoint, &QPushButton::clicked, this, &MainWindow::handleEditPoint);
    connect(ui->btnMarkReviewed, &QPushButton::clicked, this, &MainWindow::handleMarkReviewed);
    connect(ui->btnDeletePoint, &QPushButton::clicked, this, &MainWindow::handleDeletePoint);
    connect(ui->btnExportData, &QPushButton::clicked, this, &MainWindow::handleExportData);
    connect(ui->btnClearSearch, &QPushButton::clicked, this, &MainWindow::handleClearSearch);

    connect(ui->listKnowledgePoints, &QListWidget::itemSelectionChanged,
            this, &MainWindow::handleListSelectionChanged);
    connect(ui->listKnowledgePoints, &QListWidget::itemDoubleClicked,
            this, &MainWindow::handleListItemDoubleClicked);
    connect(ui->editSearch, &QLineEdit::textChanged,
            this, &MainWindow::handleSearchTextChanged);
    connect(ui->comboFilterCategory, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::handleFilterCategoryChanged);
    connect(ui->comboFilterStatus, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::handleFilterStatusChanged);
    connect(ui->comboStatus, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::handleStatusChanged);
    connect(ui->calendarReview, &QCalendarWidget::clicked,
            this, &MainWindow::handleCalendarClicked);

    qDebug() << "MainWindow initialization completed";
}


//MainWindow类的析构函数 自动执行，用于销毁窗口
MainWindow::~MainWindow()
{
    saveKnowledgePoints();
    delete m_imageViewer; // 释放图片查看器
    delete ui;
}

// 以下是所有槽函数的实现，只需要重命名即可

// 槽函数：响应“添加”按钮点击，弹出对话框让用户输入新知识点信息
void MainWindow::handleAddNew()
{
    qDebug() << "handleAddNew called";

    bool ok;
    QString title = QInputDialog::getText(this, "添加知识点", "请输入知识点问题:",
                                          QLineEdit::Normal, "", &ok);
    if (!ok) {
        qDebug() << "Add new cancelled by user";
        return;
    }
    if (title.isEmpty()) {
        qDebug() << "User entered empty title";
        QMessageBox::warning(this, "错误", "知识点标题不能为空!");
        return;
    }

    QString content = QInputDialog::getMultiLineText(this, "添加知识点",
                                                     "请输入知识点内容:", "", &ok);
    if (!ok) {
        qDebug() << "Add new cancelled at content stage";
        return;
    }

    QStringList imagePaths;
    if (QMessageBox::question(this, "添加图片", "是否要添加图片?") == QMessageBox::Yes) {
        // 改为多文件选择
        QStringList selectedFiles = QFileDialog::getOpenFileNames(
            this, "选择图片（可多选）", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff)");
        for (const QString &file : selectedFiles) {
            if (!file.isEmpty()) {
                QString stored = copyImageToStorage(file);
                imagePaths.append(stored);
            }
        }
    }

    QString category = QInputDialog::getText(this, "添加分类", "请输入分类名称:",
                                             QLineEdit::Normal, "未分类", &ok);
    if (!ok) {
        category = "未分类";
    }

    addKnowledgePoint(title, content, imagePaths, category);
    qDebug() << "Add new completed";
}
// void MainWindow::handleAddNew()
// {
//     qDebug() << "handleAddNew called";

//     bool ok;
//     //弹出对话框赋值
//     QString title = QInputDialog::getText(this, "添加知识点", "请输入知识点问题:",
//                                           QLineEdit::Normal, "", &ok);  //这里是输入好标题后点击ok

//     if (!ok) {
//         qDebug() << "Add new cancelled by user";
//         return;
//     }

//     if (title.isEmpty()) {
//         qDebug() << "User entered empty title";
//         QMessageBox::warning(this, "错误", "知识点标题不能为空!");
//         return;
//     }

//     QString content = QInputDialog::getMultiLineText(this, "添加知识点",
//                                                      "请输入知识点内容:", "", &ok);
//     if (!ok) {
//         qDebug() << "Add new cancelled at content stage";
//         return;
//     }

//     QString imagePath;
//     if (QMessageBox::question(this, "添加图片", "是否要添加图片?") == QMessageBox::Yes) {  //这里返回值是yes,或者no，方便之后比对
//         QString selectedImagePath = QFileDialog::getOpenFileName(this, "选择图片", "",
//                                                                  "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff)");//返回完整路径，并只筛选如下几个类型
//         if (!selectedImagePath.isEmpty()) {
//             // 复制图片到专用存储目录
//             imagePath = copyImageToStorage(selectedImagePath);
//             qDebug() << "Selected image:" << selectedImagePath << "-> Stored at:" << imagePath;
//         }
//     }

//     QString category = QInputDialog::getText(this, "添加分类", "请输入分类名称:",
//                                              QLineEdit::Normal, "未分类", &ok);
//     if (!ok) {
//         category = "未分类";
//     }

//     addKnowledgePoint(title, content, imagePath, category);
//     qDebug() << "Add new completed";
// }

// 槽函数：编辑当前选中的知识点
void MainWindow::handleEditPoint()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) {
        QMessageBox::information(this, "提示", "请先选择一个知识点");
        return;
    }

    int id = currentItem->data(Qt::UserRole).toInt();
    if (!knowledgePoints.contains(id)) {
        QMessageBox::warning(this, "错误", "选中的知识点不存在!");
        return;
    }

    const KnowledgePoint &point = knowledgePoints[id];

    bool ok;
    // QString title = QInputDialog::getText(this, "编辑知识点", "修改标题:",
    //                                       QLineEdit::Normal, point.title, &ok);
    // if (!ok) return;
    QString title = QInputDialog::getMultiLineText(this, "编辑知识点", "修改标题:",
                                           point.title, &ok);
    if (!ok) return;

    QString content = QInputDialog::getMultiLineText(this, "编辑知识点",
                                                     "修改内容:", point.content, &ok);
    if (!ok) return;

    QStringList newImagePaths = point.imagePaths; // 先保留原有图片
    if (QMessageBox::question(this, "修改图片", "是否要修改图片?") == QMessageBox::Yes) {
        // 询问是清空重选还是追加
        int ret = QMessageBox::question(this, "编辑图片", "是否清空已有图片？\n（选择“是”将清空所有已有图片，再选择新图片）",
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            // 删除旧图片文件
            for (const QString &oldPath : newImagePaths) {
                if (!oldPath.isEmpty() && oldPath.startsWith(m_imageStoragePath)) {
                    QFile::remove(oldPath);
                }
            }
            newImagePaths.clear();
        }

        QStringList selectedFiles = QFileDialog::getOpenFileNames(
            this, "选择新图片（可多选）", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff)");
        for (const QString &file : selectedFiles) {
            if (!file.isEmpty()) {
                newImagePaths.append(copyImageToStorage(file));
            }
        }
    }

    QString category = QInputDialog::getText(this, "修改分类", "修改分类名称:",
                                             QLineEdit::Normal, point.category, &ok);
    if (!ok) {
        category = point.category;
    }

    editKnowledgePoint(id, title, content, newImagePaths, category);
}
// void MainWindow::handleEditPoint()
// {
//     QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
//     if (!currentItem) {
//         QMessageBox::information(this, "提示", "请先选择一个知识点");
//         return;
//     }

//     int id = currentItem->data(Qt::UserRole).toInt();
//     if (!knowledgePoints.contains(id)) {
//         QMessageBox::warning(this, "错误", "选中的知识点不存在!");
//         return;
//     }

//     const KnowledgePoint &point = knowledgePoints[id];

//     bool ok;
//     QString title = QInputDialog::getText(this, "编辑知识点", "修改标题:",
//                                           QLineEdit::Normal, point.title, &ok);
//     if (!ok) return;

//     QString content = QInputDialog::getMultiLineText(this, "编辑知识点",
//                                                      "修改内容:", point.content, &ok);
//     if (!ok) return;

//     QString imagePath = point.imagePaths;
//     if (QMessageBox::question(this, "修改图片", "是否要修改图片?") == QMessageBox::Yes) {
//         QString selectedImagePath = QFileDialog::getOpenFileName(this, "选择图片", "",
//                                                                  "Images (*.png *.jpg *.jpeg *.bmp *.gif *.tiff)");
//         if (!selectedImagePath.isEmpty()) {
//             // 复制新图片到专用存储目录
//             imagePath = copyImageToStorage(selectedImagePath);
//             qDebug() << "New image selected:" << selectedImagePath << "-> Stored at:" << imagePath;

//             // 可选：删除旧的图片文件（如果它在专用目录中）
//             if (!point.imagePaths.isEmpty() && point.imagePaths.startsWith(m_imageStoragePath)) {
//                 QFile oldFile(point.imagePaths);
//                 if (oldFile.exists()) {
//                     oldFile.remove();
//                     qDebug() << "Old image removed:" << point.imagePaths;
//                 }
//             }
//         }
//     }

//     QString category = QInputDialog::getText(this, "修改分类", "修改分类名称:",
//                                              QLineEdit::Normal, point.category, &ok);
//     if (!ok) {
//         category = point.category;
//     }

//     editKnowledgePoint(id, title, content, imagePath, category);
// }

// 槽函数：标记复习（已被三个具体按钮替代，实际未使用）
void MainWindow::handleMarkReviewed()
{
    qDebug() << "handleMarkReviewed called";

    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) {
        qDebug() << "No item selected";
        QMessageBox::warning(this, "提示", "请先选择一个知识点进行复习!");
        return;
    }

    int id = currentItem->data(Qt::UserRole).toInt();
    qDebug() << "Selected item ID:" << id;

    if (!knowledgePoints.contains(id)) {
        qDebug() << "Knowledge point not found for ID:" << id;
        QMessageBox::warning(this, "错误", "选中的知识点不存在!");
        return;
    }
    int reviewvalue=0;
    qDebug() << "Marking as reviewed...";
    // markAsReviewed(id,reviewvalue);//功能由三个熟悉，模糊，忘记代码替代
    qDebug() << "Mark as reviewed completed";
}


//用于删除知识点
void MainWindow::handleDeletePoint()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return;

    int id = currentItem->data(Qt::UserRole).toInt();
    if (!knowledgePoints.contains(id)) return;

    // 获取知识点的所有图片路径（删除前保存，避免移除后丢失）
    QStringList imagesToDelete = knowledgePoints[id].imagePaths;

    if (QMessageBox::question(this, "确认删除", "确定要删除这个知识点吗?") == QMessageBox::Yes) {
        // 删除所有关联的图片文件
        for (const QString &imgPath : imagesToDelete) {
            if (!imgPath.isEmpty()) {
                QFile imageFile(imgPath);
                if (imageFile.exists()) {
                    imageFile.remove();
                }
            }
        }

        // 从知识库中移除
        knowledgePoints.remove(id);
        saveKnowledgePoints();
        refreshKnowledgeList();
        updateStatistics();

        // 清空界面显示
        ui->textContent->clear();
        ui->textquestion->clear();           // 同时清空问题显示
        displayImages(QStringList());        // 清空多图区域（显示占位文字“无图片”）
        ui->progressMastery->setValue(0);
        ui->labelMasteryPercen->setText("0%");
        ui->labelLastReviewValue->setText("");
        ui->labelNextReviewValue->setText("");
    }
}
// void MainWindow::handleDeletePoint()
// {
//     QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
//     if (!currentItem) return;

//     int id = currentItem->data(Qt::UserRole).toInt();
//     // 在删除前先获取知识点的图片文件名
//     QString imageFileName;
//     qDebug() << imageFileName;
//     if (knowledgePoints.contains(id)) {
//         imageFileName = knowledgePoints[id].imagePaths;
//     }
//     if (QMessageBox::question(this, "确认删除", "确定要删除这个知识点吗?") == QMessageBox::Yes) {
//         // 删除对应的图片文件
//         if (!imageFileName.isEmpty()) {
//             QFile imageFile(imageFileName);
//             if (imageFile.exists()) {
//                 imageFile.remove();
//             }
//         }
//         knowledgePoints.remove(id);
//         saveKnowledgePoints();
//         refreshKnowledgeList();
//         updateStatistics();

//         // 清空显示
//         ui->textContent->clear();
//         ui->scrollImageArea->setText("图片显示");
//         ui->progressMastery->setValue(0);
//         ui->labelMasteryPercen->setText("0%");
//     }
// }

//导出json数据，方便换程序，其实直接复制文件夹下的也不错
void MainWindow::handleExportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出数据", "",
                                                    "JSON Files (*.json)");
    if (fileName.isEmpty()) return;

    QJsonArray jsonArray;
    for (const auto &point : knowledgePoints) {
        QJsonObject jsonObject;
        jsonObject["id"] = point.id;
        jsonObject["title"] = point.title;
        jsonObject["content"] = point.content;

        // 导出多图片数组
        QJsonArray imgs;
        for (const QString &p : point.imagePaths)
            imgs.append(p);
        jsonObject["imagePaths"] = imgs;
        // 为了兼容旧版本同时保留单个字段（可选）
        if (!point.imagePaths.isEmpty())
            jsonObject["imagePath"] = point.imagePaths.first();

        jsonObject["category"] = point.category;
        jsonObject["status"] = static_cast<int>(point.status);
        jsonObject["masteryLevel"] = point.masteryLevel;
        jsonObject["createDate"] = point.createDate.toString(Qt::ISODate);
        jsonObject["lastReviewDate"] = point.lastReviewDate.toString(Qt::ISODate);
        jsonObject["nextReviewDate"] = point.nextReviewDate.toString(Qt::ISODate);
        jsonObject["reviewCount"] = point.reviewCount;
        jsonObject["memoryStability"] = point.memoryStability;

        jsonArray.append(jsonObject);
    }

    QJsonDocument doc(jsonArray);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        QMessageBox::information(this, "导出成功", "数据导出成功!");
    } else {
        QMessageBox::warning(this, "导出失败", "无法保存文件!");
    }
}
// void MainWindow::handleExportData()
// {
//     QString fileName = QFileDialog::getSaveFileName(this, "导出数据", "",
//                                                     "JSON Files (*.json)");
//     if (fileName.isEmpty()) return;

//     QJsonArray jsonArray;
//     for (const auto &point : knowledgePoints) {
//         QJsonObject jsonObject;
//         jsonObject["id"] = point.id;
//         jsonObject["title"] = point.title;
//         jsonObject["content"] = point.content;
//         jsonObject["imagePath"] = point.imagePaths;
//         jsonObject["category"] = point.category;
//         jsonObject["status"] = static_cast<int>(point.status);
//         jsonObject["masteryLevel"] = point.masteryLevel;
//         jsonObject["createDate"] = point.createDate.toString(Qt::ISODate);
//         jsonObject["lastReviewDate"] = point.lastReviewDate.toString(Qt::ISODate);
//         jsonObject["nextReviewDate"] = point.nextReviewDate.toString(Qt::ISODate);
//         jsonObject["reviewCount"] = point.reviewCount;

//         jsonArray.append(jsonObject);
//     }

//     QJsonDocument doc(jsonArray);
//     QFile file(fileName);
//     if (file.open(QIODevice::WriteOnly)) {
//         file.write(doc.toJson());
//         file.close();
//         QMessageBox::information(this, "导出成功", "数据导出成功!");
//     } else {
//         QMessageBox::warning(this, "导出失败", "无法保存文件!");
//     }
// }


//处理搜索
void MainWindow::handleClearSearch()
{
    ui->editSearch->clear();
    ui->comboFilterCategory->setCurrentIndex(0);
    ui->comboFilterStatus->setCurrentIndex(0);

    currentSearchText = "";
    currentCategoryFilter = "";
    currentStatusFilter = "";

    filterKnowledgePoints();
}

// 槽函数：列表选中项变化时，在右侧显示对应知识点的详细信息
void MainWindow::handleListSelectionChanged()
{
    if (m_isReviewMode) {
        // 复习模式下只允许程序内部切换，忽略用户的手动选择
        return;
    }
    if (m_isRefreshing) {
        qDebug() << "Currently refreshing, skipping selection change";
        return;
    }

    static bool inSelectionChange = false; // 防止递归的标志

    if (inSelectionChange) {
        qDebug() << "Already in selection change, skipping";
        return;
    }

    inSelectionChange = true;

    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) {
        qDebug() << "No item selected";
        // 清空显示，避免显示无效数据
        ui->textContent->clear();
        displayImages(QStringList());      // 清空多图区域，显示占位“无图片”
        ui->progressMastery->setValue(0);
        ui->labelMasteryPercen->setText("0%");
        ui->labelLastReviewValue->setText("");
        ui->labelNextReviewValue->setText("");
        inSelectionChange = false;
        return;
    }

    int id = currentItem->data(Qt::UserRole).toInt();
    qDebug() << "Selected item ID:" << id;

    showKnowledgePointDetails(id);

    inSelectionChange = false;
    qDebug() << "handleListSelectionChanged completed";
}
// void MainWindow::handleListSelectionChanged()
// {
//     if (m_isReviewMode) {
//         // 复习模式下只允许程序内部切换，忽略用户的手动选择
//         return;
//     }
//     if (m_isRefreshing) {
//         qDebug() << "Currently refreshing, skipping selection change";
//         return;
//     }

//     qDebug() << "handleListSelectionChanged called";

//     static bool inSelectionChange = false; // 防止递归的标志

//     if (inSelectionChange) {
//         qDebug() << "Already in selection change, skipping";
//         return;
//     }

//     inSelectionChange = true;

//     qDebug() << "handleListSelectionChanged called";

//     QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
//     if (!currentItem) {
//         qDebug() << "No item selected";
//         // 清空显示，避免显示无效数据
//         ui->textContent->clear();
//         ui->labelImageDisplay->setText("图片显示");
//         ui->progressMastery->setValue(0);
//         ui->labelMasteryPercen->setText("0%");
//         ui->labelLastReviewValue->setText("");
//         ui->labelNextReviewValue->setText("");
//         inSelectionChange = false;
//         return;
//     }

//     int id = currentItem->data(Qt::UserRole).toInt();
//     qDebug() << "Selected item ID:" << id;

//     showKnowledgePointDetails(id);

//     inSelectionChange = false;
//     qDebug() << "handleListSelectionChanged completed";

//     qDebug() << "handleListSelectionChanged completed";
// }


// 槽函数：双击列表项，触发复习（实际由熟悉/模糊/忘记按钮处理）
void MainWindow::handleListItemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    handleMarkReviewed();
}

// 槽函数：搜索框文本变化，执行过滤
void MainWindow::handleSearchTextChanged(const QString &text)
{
    currentSearchText = text;
    filterKnowledgePoints();
}

// 槽函数：分类过滤器变化，执行过滤
void MainWindow::handleFilterCategoryChanged(int index)
{
    currentCategoryFilter = ui->comboFilterCategory->itemData(index).toString();
    filterKnowledgePoints();
}

// 槽函数：状态过滤器变化，执行过滤
void MainWindow::handleFilterStatusChanged(int index)
{
    currentStatusFilter = ui->comboFilterStatus->itemData(index).toString();
    filterKnowledgePoints();
}

// 槽函数：详情区状态下拉框改变时，更新知识点的状态
void MainWindow::handleStatusChanged(int index)
{
    qDebug() << "handleStatusChanged called with index:" << index;

    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) {
        qDebug() << "No item selected, ignoring status change";
        return;
    }

    int id = currentItem->data(Qt::UserRole).toInt();
    if (!knowledgePoints.contains(id)) {
        qDebug() << "Knowledge point not found for ID:" << id;
        return;
    }

    // 检查索引是否有效
    if (index < 0 || index >= ui->comboStatus->count()) {
        qDebug() << "Invalid combo box index:" << index;
        return;
    }

    KnowledgePoint &point = knowledgePoints[id];
    KnowledgeStatus newStatus = static_cast<KnowledgeStatus>(ui->comboStatus->itemData(index).toInt());

    qDebug() << "Changing status from" << point.status << "to" << newStatus;

    point.status = newStatus;

    saveKnowledgePoints();
    refreshKnowledgeList();
    updateStatistics();

    qDebug() << "Status change completed";
}


//处理高亮
void MainWindow::handleCalendarClicked(const QDate &date)
{
    // 1. 先清除之前所有日期的高亮（避免颜色重叠或残留）
    ui->calendarReview->setDateTextFormat(QDate(), QTextCharFormat());

    // 2. 为所有需要复习的知识点设置高亮
    QTextCharFormat reviewFormat;
    reviewFormat.setBackground(Qt::yellow);

    QDate today = QDate::currentDate();
    for (const auto &point : knowledgePoints) {
        // 仅高亮未来(或今天)的复习日期，且知识点未掌握
        if (point.nextReviewDate.isValid() &&
            point.nextReviewDate >= today &&
            point.status != STATUS_MASTERED)
        {
            ui->calendarReview->setDateTextFormat(point.nextReviewDate, reviewFormat);
        }
    }

    // 3. 如果你还想特别标出“被点击的这一天”上的复习任务，可以用另一种颜色
    // 例如：红色高亮被点击的日期
    QTextCharFormat clickedFormat;
    clickedFormat.setBackground(Qt::red);
    ui->calendarReview->setDateTextFormat(date, clickedFormat);
}


//打开时载入知识点
void MainWindow::loadKnowledgePoints()
{
    qDebug() << "Loading knowledge points from JSON...";

    // 构建 JSON 文件路径（可执行文件同目录下的 knowledge.json）
    QString filePath = QCoreApplication::applicationDirPath() + "/knowledge.json";
    qDebug() << "JSON file path:" << filePath;

    QFile file(filePath);
    if (!file.exists()) {
        qDebug() << "No knowledge.json found, starting with empty data.";
        knowledgePoints.clear();
        nextId = 1;
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open knowledge.json:" << file.errorString();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return;
    }

    if (!doc.isArray()) {
        qWarning() << "JSON root is not an array!";
        return;
    }

    QJsonArray array = doc.array();
    qDebug() << "Found" << array.size() << "knowledge points in JSON";

    knowledgePoints.clear();
    nextId = 1;

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();

        // 检查必要字段是否存在
        if (!obj.contains("id") || !obj.contains("title")) {
            qDebug() << "Skipping invalid entry (missing id or title)";
            continue;
        }

        KnowledgePoint point;
        point.id = obj["id"].toInt();
        point.title = obj["title"].toString();
        point.content = obj["content"].toString();
        // point.imagePath = obj["imagePath"].toString();
        if (obj.contains("imagePath") && obj["imagePath"].isString()) {//多图读取
            QString oldPath = obj["imagePath"].toString();
            if (!oldPath.isEmpty())
                point.imagePaths.append(oldPath);
        }
        // 新格式（图片路径数组）
        if (obj.contains("imagePaths") && obj["imagePaths"].isArray()) {
            QJsonArray arr = obj["imagePaths"].toArray();
            for (const QJsonValue &v : arr) {
                QString p = v.toString();
                if (!p.isEmpty())
                    point.imagePaths.append(p);
            }
        }
        point.category = obj["category"].toString();
        point.status = static_cast<KnowledgeStatus>(obj["status"].toInt());
        point.masteryLevel = obj["masteryLevel"].toInt();
        point.createDate = QDate::fromString(obj["createDate"].toString(), Qt::ISODate);
        point.lastReviewDate = QDate::fromString(obj["lastReviewDate"].toString(), Qt::ISODate);
        point.nextReviewDate = QDate::fromString(obj["nextReviewDate"].toString(), Qt::ISODate);
        point.reviewCount = obj["reviewCount"].toInt();
        // ✅ 添加以下代码：读取记忆稳定性，兼容旧数据
        if (obj.contains("memoryStability")) {
            point.memoryStability = obj["memoryStability"].toDouble();
        } else {
            point.memoryStability = 1.0;   // 旧知识点默认稳定性
        }

        // 验证数据有效性
        if (point.id <= 0 || point.title.isEmpty()) {
            qDebug() << "Skipping invalid knowledge point:" << point.id << point.title;
            continue;
        }

        knowledgePoints[point.id] = point;
        if (point.id >= nextId) nextId = point.id + 1;

        qDebug() << "Loaded point:" << point.id << point.title;
    }

    qDebug() << "Total loaded:" << knowledgePoints.size() << "valid knowledge points";
}


//知识点写入，每次都是整个写入读取
void MainWindow::saveKnowledgePoints()
{
    qDebug() << "Saving" << knowledgePoints.size() << "knowledge points to JSON...";

    // 阻塞所有可能触发刷新的信号
    bool oldListState = ui->listKnowledgePoints->blockSignals(true);
    bool oldComboState = ui->comboStatus->blockSignals(true);

    // 将知识点列表按下次复习时间排序（由近到远）
    QList<KnowledgePoint> sortedPoints = knowledgePoints.values();

    std::sort(sortedPoints.begin(), sortedPoints.end(),
              [](const KnowledgePoint &a, const KnowledgePoint &b) {
                  return a.nextReviewDate < b.nextReviewDate;
              });

    // 构建 JSON 数组
    QJsonArray jsonArray;
    for (const auto &point : sortedPoints) {
        QJsonObject obj;
        obj["id"] = point.id;
        obj["title"] = point.title;
        obj["content"] = point.content;
        // obj["imagePath"] = point.imagePath;
        QJsonArray imgArray;
        for (const QString &path : point.imagePaths) {  //多图片
            imgArray.append(path);
        }
        obj["imagePaths"] = imgArray;   // 保存为数组
        obj["category"] = point.category;
        obj["status"] = static_cast<int>(point.status);
        obj["masteryLevel"] = point.masteryLevel;
        obj["createDate"] = point.createDate.toString(Qt::ISODate);
        obj["lastReviewDate"] = point.lastReviewDate.toString(Qt::ISODate);
        obj["nextReviewDate"] = point.nextReviewDate.toString(Qt::ISODate);
        obj["reviewCount"] = point.reviewCount;
        obj["memoryStability"] = point.memoryStability;

        jsonArray.append(obj);

        qDebug() << "Prepared point for saving:" << point.id << point.title;
    }

    QJsonDocument doc(jsonArray);

    // 写入文件
    QString filePath = QCoreApplication::applicationDirPath() + "/knowledge.json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qDebug() << "Data saved successfully to" << filePath;

        // ---- 新增：每日备份逻辑 ----
        QDate today = QDate::currentDate();
        if (m_lastBackupDate != today) {
            // 首次备份前先清理旧文件
            cleanOldBackups();

            QString backupDir = getBackupPath();
            QString backupFilePath = backupDir + QString("/knowledge_%1.json").arg(today.toString("yyyy-MM-dd"));

            if (QFile::copy(filePath, backupFilePath)) {
                qDebug() << "Backup created:" << backupFilePath;
                m_lastBackupDate = today;   // 更新最后备份日期
            } else {
                qWarning() << "Failed to create backup!";
            }
        }
        // ---- 备份结束 ----
    } else {
        qWarning() << "Failed to save knowledge.json:" << file.errorString();
    }

    // 恢复信号状态
    ui->listKnowledgePoints->blockSignals(oldListState);
    ui->comboStatus->blockSignals(oldComboState);
}


// 获取备份文件夹路径（程序目录下的 backups）

QString MainWindow::getBackupPath() const
{
    QString path = QCoreApplication::applicationDirPath() + "/backups";
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return path;
}

// 清理旧备份，只保留最近 7 天的文件

void MainWindow::cleanOldBackups() const
{
    QString backupDir = getBackupPath();
    QDir dir(backupDir);
    QStringList filters;
    filters << "knowledge_*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    QDate cutoff = QDate::currentDate().addDays(-7);
    for (const QFileInfo &info : files) {
        // 从文件名提取日期，格式：knowledge_2026-04-25.json
        QString name = info.baseName();           // knowledge_2026-04-25
        QString dateStr = name.mid(10);           // 2026-04-25 （"knowledge_"长度为10）
        QDate fileDate = QDate::fromString(dateStr, "yyyy-MM-dd");
        if (fileDate.isValid() && fileDate < cutoff) {
            QFile::remove(info.absoluteFilePath());
            qDebug() << "Removed old backup:" << info.fileName();
        }
    }
}

// 刷新左侧知识点列表（应用当前搜索和过滤条件）
void MainWindow::refreshKnowledgeList()
{
    if (m_isRefreshing) {
        qDebug() << "Already refreshing, skipping recursive call";
        return;
    }

    m_isRefreshing = true;
    qDebug() << "refreshKnowledgeList called";

    // 阻塞信号，防止触发选择变化事件
    bool oldState = ui->listKnowledgePoints->blockSignals(true);

    // 保存当前选中的项目
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    int currentId = -1;
    if (currentItem) {
        currentId = currentItem->data(Qt::UserRole).toInt();
    }

    ui->listKnowledgePoints->clear();
    qDebug() << "List cleared";

    // 获取所有分类并更新过滤器
    QSet<QString> categories;
    ui->comboFilterCategory->clear();
    ui->comboFilterCategory->addItem("全部分类", "");

    for (const auto &point : knowledgePoints) {
        if (!point.category.isEmpty() && !categories.contains(point.category)) {
            categories.insert(point.category);
            ui->comboFilterCategory->addItem(point.category, point.category);
        }
    }
    qDebug() << "Categories updated:" << categories.size();

    // 添加知识点到列表
    int addedCount = 0;
    QListWidgetItem *selectedItem = nullptr;

    for (const auto &point : knowledgePoints) {
        // 应用过滤器
        if (!currentSearchText.isEmpty() &&
            !point.title.contains(currentSearchText, Qt::CaseInsensitive) &&
            !point.content.contains(currentSearchText, Qt::CaseInsensitive)) {
            continue;
        }

        if (!currentCategoryFilter.isEmpty() && point.category != currentCategoryFilter) {
            continue;
        }

        if (!currentStatusFilter.isEmpty()) {
            QString statusStr;
            switch (point.status) {
            case STATUS_NEW: statusStr = "new"; break;
            case STATUS_LEARNING: statusStr = "learning"; break;
            case STATUS_REVIEWING: statusStr = "reviewing"; break;
            case STATUS_MASTERED: statusStr = "mastered"; break;
            }
            if (statusStr != currentStatusFilter) continue;
        }

        QListWidgetItem *item = new QListWidgetItem(point.title);
        item->setData(Qt::UserRole, point.id);

        // 根据状态设置颜色
        switch (point.status) {
        case STATUS_NEW:
            item->setBackground(Qt::lightGray);
            break;
        case STATUS_LEARNING:
            item->setBackground(QColor(255, 255, 200)); // 浅黄色
            break;
        case STATUS_REVIEWING:
            if (point.nextReviewDate <= QDate::currentDate()) {
                item->setBackground(QColor(255, 200, 200)); // 浅红色（需要复习）
            } else {
                item->setBackground(QColor(200, 255, 200)); // 浅绿色
            }
            break;
        case STATUS_MASTERED:
            item->setBackground(Qt::cyan);
            break;
        }

        ui->listKnowledgePoints->addItem(item);
        addedCount++;

        // 记录需要选中的项目
        if (point.id == currentId) {
            selectedItem = item;
        }
    }

    qDebug() << "Added" << addedCount << "items to list";

    // 恢复选中状态
    if (selectedItem) {
        selectedItem->setSelected(true);
        ui->listKnowledgePoints->setCurrentItem(selectedItem);
        qDebug() << "Restored selection to item ID:" << currentId;
    } else if (ui->listKnowledgePoints->count() > 0) {
        // 如果没有匹配的选中项目，选择第一个
        ui->listKnowledgePoints->setCurrentRow(0);
        qDebug() << "Auto-selected first item";
    }

    // 恢复信号
    ui->listKnowledgePoints->blockSignals(oldState);

    qDebug() << "refreshKnowledgeList completed";

    m_isRefreshing = false;
    qDebug() << "refreshKnowledgeList completed";
}


// 更新统计信息（总计、待复习、学习中、已掌握）
void MainWindow::updateStatistics()
{
    qDebug() << "updateStatistics called";

    int total = knowledgePoints.size();
    int due = 0;
    int learning = 0;
    int mastered = 0;

    QDate today = QDate::currentDate();

    for (const auto &point : knowledgePoints) {
        if (point.status == STATUS_LEARNING) learning++;
        else if (point.status == STATUS_MASTERED) mastered++;
        else if (point.status == STATUS_REVIEWING && point.nextReviewDate <= today) due++;
    }

    ui->labelStatsTotal->setText(QString("总计：%1").arg(total));
    ui->labelStatsDue->setText(QString("待复习：%1").arg(due));
    ui->label_3->setText(QString("学习中：%1").arg(learning));
    ui->label_4->setText(QString("已掌握：%1").arg(mastered));

    qDebug() << "Statistics: Total:" << total << "Due:" << due << "Learning:" << learning << "Mastered:" << mastered;
    qDebug() << "updateStatistics completed";
}


// 在右侧详情区域显示指定ID的知识点信息

void MainWindow::showKnowledgePointDetails(int id)
{
    qDebug() << "showKnowledgePointDetails called with ID:" << id;

    if (!knowledgePoints.contains(id)) {
        qDebug() << "Error: Knowledge point not found in showDetails!";
        // 清空显示，避免显示无效数据
        ui->textContent->clear();
        displayImages(QStringList());          // 清空多图区域，显示“无图片”占位
        ui->progressMastery->setValue(0);
        ui->labelMasteryPercen->setText("0%");
        ui->labelLastReviewValue->setText("");
        ui->labelNextReviewValue->setText("");
        return;
    }

    const KnowledgePoint &point = knowledgePoints[id];
    qDebug() << "Showing details for:" << point.title;

    // 显示基本信息
    ui->textquestion->setPlainText(point.title);
    ui->textContent->setPlainText(point.content);
    hideContentInTextEdit();

    // 显示图片（多图）
    displayImages(point.imagePaths);

    // 显示掌握程度
    ui->progressMastery->setValue(point.masteryLevel);
    ui->labelMasteryPercen->setText(QString("%1%").arg(point.masteryLevel));

    // 显示状态 - 阻塞信号避免递归
    bool oldState = ui->comboStatus->blockSignals(true);
    int statusIndex = -1;
    for (int i = 0; i < ui->comboStatus->count(); ++i) {
        if (ui->comboStatus->itemData(i).toInt() == static_cast<int>(point.status)) {
            statusIndex = i;
            break;
        }
    }
    if (statusIndex >= 0) {
        ui->comboStatus->setCurrentIndex(statusIndex);
    } else {
        ui->comboStatus->setCurrentIndex(0);
    }
    ui->comboStatus->blockSignals(oldState);

    // 显示复习时间
    ui->labelLastReviewValue->setText(point.lastReviewDate.isValid() ?
                                          point.lastReviewDate.toString("yyyy-MM-dd") : "从未复习");
    ui->labelNextReviewValue->setText(point.nextReviewDate.isValid() ?
                                          point.nextReviewDate.toString("yyyy-MM-dd") : "未设置");

    qDebug() << "Details shown successfully";
}
// void MainWindow::showKnowledgePointDetails(int id)
// {
//     qDebug() << "showKnowledgePointDetails called with ID:" << id;

//     if (!knowledgePoints.contains(id)) {
//         qDebug() << "Error: Knowledge point not found in showDetails!";
//         // 清空显示，避免显示无效数据
//         ui->textContent->clear();
//         ui->labelImageDisplay->setText("无数据");
//         ui->progressMastery->setValue(0);
//         ui->labelMasteryPercen->setText("0%");
//         ui->labelLastReviewValue->setText("");
//         ui->labelNextReviewValue->setText("");
//         return;
//     }

//     const KnowledgePoint &point = knowledgePoints[id];
//     qDebug() << "Showing details for:" << point.title;

//     // 显示基本信息
//     ui->textquestion->setPlainText(point.title);
//     ui->textContent->setPlainText(point.content);
//     hideContentInTextEdit();

//     // 显示图片
//     displayImage(point.imagePath);

//     // 显示掌握程度
//     ui->progressMastery->setValue(point.masteryLevel);
//     ui->labelMasteryPercen->setText(QString("%1%").arg(point.masteryLevel));

//     // 显示状态 - 阻塞信号避免递归
//     bool oldState = ui->comboStatus->blockSignals(true);
//     int statusIndex = -1;
//     for (int i = 0; i < ui->comboStatus->count(); ++i) {
//         if (ui->comboStatus->itemData(i).toInt() == static_cast<int>(point.status)) {
//             statusIndex = i;
//             break;
//         }
//     }
//     if (statusIndex >= 0) {
//         ui->comboStatus->setCurrentIndex(statusIndex);
//     } else {
//         ui->comboStatus->setCurrentIndex(0);
//     }
//     ui->comboStatus->blockSignals(oldState);

//     // 显示复习时间
//     ui->labelLastReviewValue->setText(point.lastReviewDate.isValid() ?
//                                           point.lastReviewDate.toString("yyyy-MM-dd") : "从未复习");
//     ui->labelNextReviewValue->setText(point.nextReviewDate.isValid() ?
//                                           point.nextReviewDate.toString("yyyy-MM-dd") : "未设置");

//     qDebug() << "Details shown successfully";
// }


//添加知识点模块
void MainWindow::addKnowledgePoint(const QString &title, const QString &content,
                                   const QStringList &imagePath, const QString &category)
{
    qDebug() << "addKnowledgePoint called with title:" << title;

    if (title.isEmpty()) {
        qDebug() << "Cannot add knowledge point with empty title";
        QMessageBox::warning(this, "错误", "知识点标题不能为空!");
        return;
    }

    KnowledgePoint point;
    point.id = nextId++;
    point.title = title;
    point.content = content;
    point.imagePaths = imagePath;
    point.category = category;
    point.status = STATUS_NEW;
    point.masteryLevel = 0;
    point.createDate = QDate::currentDate();
    point.lastReviewDate = QDate();
    point.nextReviewDate = QDate::currentDate().addDays(1);
    point.reviewCount = 0;

    knowledgePoints[point.id] = point;
    qDebug() << "Point added to map, ID:" << point.id << "Total points now:" << knowledgePoints.size();

    // 立即保存数据
    saveKnowledgePoints();
    qDebug() << "saveKnowledgePoints completed";

    // 刷新界面
    refreshKnowledgeList();
    qDebug() << "refreshKnowledgeList completed";

    updateStatistics();
    qDebug() << "updateStatistics completed";

    qDebug() << "addKnowledgePoint completed for:" << point.id << point.title;
}


// 编辑指定ID的知识点信息
void MainWindow::editKnowledgePoint(int id, const QString &title, const QString &content,
                                    const QStringList &imagePath, const QString &category)
{
    if (!knowledgePoints.contains(id)) return;

    KnowledgePoint &point = knowledgePoints[id];
    point.title = title;
    point.content = content;
    point.imagePaths = imagePath;
    point.category = category;

    // 不再立即保存
    saveKnowledgePoints();   // 立即持久化
    refreshKnowledgeList();
    showKnowledgePointDetails(id);
}


// 根据复习反馈更新知识点的掌握程度和下次复习时间
// 参数 reviewvalue：熟悉+10，模糊-5，忘记-10   重要

void MainWindow::markAsReviewed(int id, int reviewvalue)
{
    qDebug() << "markAsReviewed called with ID:" << id;

    if (!knowledgePoints.contains(id)) {
        qDebug() << "Error: Knowledge point not found!";
        return;
    }

    KnowledgePoint &point = knowledgePoints[id];
    qDebug() << "Before review - Mastery:" << point.masteryLevel
             << "Stability:" << point.memoryStability;

    // 更新掌握度
    point.masteryLevel = qMax(0, point.masteryLevel + reviewvalue);

    // 根据评分更新稳定性
    QString rating;
    if (reviewvalue == 10) rating = "familiar";
    else if (reviewvalue == -5) rating = "vague";
    else if (reviewvalue == -10) rating = "forget";
    else rating = "unknown";

    updateMemoryStability(id, rating);

    // 计算下次复习日期
    point.lastReviewDate = QDate::currentDate();
    point.nextReviewDate = calculateNextReviewDate(id);

    // 更新状态
    if (point.masteryLevel >= 100) {
        point.status = STATUS_MASTERED;
    } else if (point.masteryLevel >= 50) {
        point.status = STATUS_REVIEWING;
    } else {
        point.status = STATUS_LEARNING;
    }

    point.reviewCount++;  // 统计复习次数

    qDebug() << "After review - Mastery:" << point.masteryLevel
             << "Stability:" << point.memoryStability
             << "Next review:" << point.nextReviewDate.toString("yyyy-MM-dd");

    saveKnowledgePoints();
    refreshKnowledgeList();
    updateStatistics();
    showKnowledgePointDetails(id);

    qDebug() << "markAsReviewed completed";
}
// void MainWindow::markAsReviewed(int id,int reviewvalue)
// {
//     qDebug() << "markAsReviewed called with ID:" << id;

//     if (!knowledgePoints.contains(id)) {
//         qDebug() << "Error: Knowledge point not found!";
//         return;
//     }

//     KnowledgePoint &point = knowledgePoints[id];
//     qDebug() << "Before review - Mastery:" << point.masteryLevel << "Review count:" << point.reviewCount;

//     // 1. 更新掌握程度（不再限制 100，允许超过）
//     point.masteryLevel = qMax(0, point.masteryLevel + reviewvalue);

//     // 2. 更新记忆稳定性 S（艾宾浩斯）
//     const double familiarGain = 0.5;   // 熟悉：稳定性增加 50%
//     const double vaguePenalty = 0.3;   // 模糊：稳定性减少 30%
//     const double initialStability = 1.0;
//     const double minStability = 0.1;

//     if (reviewvalue == 10) {
//         // 熟悉
//         point.memoryStability *= (1.0 + familiarGain);
//     } else if (reviewvalue == -5) {
//         // 模糊
//         point.memoryStability = qMax(minStability,
//                                      point.memoryStability * (1.0 - vaguePenalty));
//     } else if (reviewvalue == -10) {
//         // 忘记：重置稳定性
//         point.memoryStability = initialStability;
//     }

//     // 3. 根据稳定性计算下次复习日期
//     const double targetRetention = 0.9; // 当记忆保留率降至 90% 时复习
//     double intervalDays = -point.memoryStability * qLn(targetRetention);
//     int finalInterval = qMax(1, qRound(intervalDays));

//     point.lastReviewDate = QDate::currentDate();
//     point.nextReviewDate = QDate::currentDate().addDays(finalInterval);

//     // 4. 更新状态（根据新的掌握程度）
//     if (point.masteryLevel >= 100) {
//         point.status = STATUS_MASTERED;
//     } else if (point.masteryLevel >= 50) {
//         point.status = STATUS_REVIEWING;
//     } else {
//         point.status = STATUS_LEARNING;
//     }

//     // 5. 保留复习次数统计（可选）
//     point.reviewCount++;
//     // 如果需要真实复习次数（不包含重置），可自行判断

//     // point.lastReviewDate = QDate::currentDate();
//     // if(reviewvalue==-5||reviewvalue==-10){
//     //     point.reviewCount = 1;   // 重置为第一次复习后的状态
//     // } else {
//     //     point.reviewCount++;
//     // }
//     // point.reviewtureCount++;

//     // // 根据记忆曲线计算下次复习时间
//     // point.nextReviewDate = calculateNextReviewDate(point.masteryLevel, point.reviewCount);

//     // 更新掌握程度（每次复习根据记忆情况变化）
//     // int improvement = reviewvalue; // 加上熟悉，模糊，忘记的赋值
//     // point.masteryLevel = qMin(100, point.masteryLevel + improvement);

//     // qDebug() << "Improvement:" << improvement << "New mastery:" << point.masteryLevel;

//     // // 如果掌握程度达到100%，标记为已掌握
//     // if (point.masteryLevel >= 100) {
//     //     point.status = STATUS_MASTERED;
//     //     point.masteryLevel = 100;
//     //     qDebug() << "Status changed to MASTERED";
//     // } else if (point.masteryLevel >= 50) {
//     //     point.status = STATUS_REVIEWING;
//     //     qDebug() << "Status changed to REVIEWING";
//     // } else {
//     //     point.status = STATUS_LEARNING;
//     //     qDebug() << "Status changed to LEARNING";
//     // }

//     // 立即保存数据
//     saveKnowledgePoints();
//     qDebug() << "Data saved";

//     // 只刷新一次，避免递归
//     refreshKnowledgeList();
//     qDebug() << "List refreshed";

//     updateStatistics();
//     qDebug() << "Statistics updated";

//     // 显示详细信息
//     showKnowledgePointDetails(id);
//     qDebug() << "Details shown";

//     qDebug() << "markAsReviewed completed";
// }

//新记忆时间计算
QDate MainWindow::calculateNextReviewDate(int id) // 参数调整为 ID，移除 masteryLevel 和 reviewCount
{
    if (!knowledgePoints.contains(id)) return QDate::currentDate();
    KnowledgePoint &point = knowledgePoints[id];

    // 设定一个目标记忆保留率，即什么时候该复习了 (0.9 代表记住90%)
    const double targetRetention = 0.9;

    // 公式：t = -S * ln(R)
    double stability = point.memoryStability;
    double intervalDays = -stability * qLn(targetRetention); // qLn 是Qt中的自然对数

    // 安全间隔：确保复习间隔至少为 1 天
    int finalInterval = qMax(1, qRound(intervalDays));

    return QDate::currentDate().addDays(finalInterval);
}

// //原复习时间计算
// QDate MainWindow::calculateNextReviewDate(int currentLevel, int reviewCount)
// {
//     QDate nextDate = QDate::currentDate();

//     if (currentLevel < 99) {
//         // 使用预设的记忆曲线间隔
//         nextDate = nextDate.addDays(reviewIntervals[reviewCount]);
//     } else {
//         // 超过预设间隔后，根据掌握程度动态计算
//         int baseInterval = 30; // 基础间隔30天
//         int levelFactor = (currentLevel - 100) / 10; // 掌握程度越低，复习越频繁
//         nextDate = nextDate.addDays(baseInterval + levelFactor * 5);
//     }
//     // if (reviewCount < reviewIntervals.size()) {
//     //     // 使用预设的记忆曲线间隔
//     //     nextDate = nextDate.addDays(reviewIntervals[reviewCount]);
//     // } else {
//     //     // 超过预设间隔后，根据掌握程度动态计算
//     //     int baseInterval = 30; // 基础间隔30天
//     //     int levelFactor = (100 - currentLevel) / 10; // 掌握程度越低，复习越频繁
//     //     nextDate = nextDate.addDays(baseInterval + levelFactor * 5);
//     // }

//     return nextDate;
// }

//自适应艾宾浩斯算法
void MainWindow::updateMemoryStability(int id, const QString& rating)
{
    if (!knowledgePoints.contains(id)) return;
    KnowledgePoint &point = knowledgePoints[id];

    // 可调参数配置
    const double familiarFactor = 0.5; // 熟悉增益
    const double vagueFactor = 0.3;    // 模糊惩罚
    const double initialStability = 1.0; // 初始稳定性 S0
    const double minStability = 0.1;   // 稳定性最低值，防止归零

    if (rating == "familiar") {
        point.memoryStability *= (1.0 + familiarFactor);
    } else if (rating == "vague") {
        point.memoryStability = qMax(minStability, point.memoryStability * (1.0 - vagueFactor));
    } else if (rating == "forget") {
        point.memoryStability = initialStability; // 重置
    }
}



void MainWindow::updateMasteryLevel(int id, int newLevel)
{
    if (!knowledgePoints.contains(id)) return;

    KnowledgePoint &point = knowledgePoints[id];
    point.masteryLevel = newLevel;

    // 更新状态
    if (newLevel >= 100) {
        point.status = STATUS_MASTERED;
    } else if (newLevel >= 50) {
        point.status = STATUS_REVIEWING;
    } else {
        point.status = STATUS_LEARNING;
    }

    saveKnowledgePoints();
    refreshKnowledgeList();
    updateStatistics();
}

void MainWindow::filterKnowledgePoints()
{
    refreshKnowledgeList();
    updateStatistics();
}

// void MainWindow::displayImage(const QString &imagePath)
// {
//     ui->labelImageDisplay->clear();

//     if (!imagePath.isEmpty()) {
//         QFileInfo fileInfo(imagePath);
//         if (fileInfo.exists() && fileInfo.isFile()) {
//             QPixmap pixmap(imagePath);
//             if (!pixmap.isNull()) {
//                 // 应用缩放因子
//                 QSize scaledSize = pixmap.size() * imageZoomFactor;
//                 QPixmap scaledPixmap = pixmap.scaled(scaledSize,
//                                                      Qt::KeepAspectRatio,
//                                                      Qt::SmoothTransformation);
//                 ui->labelImageDisplay->setPixmap(scaledPixmap);
//                 return;
//             } else {
//                 ui->labelImageDisplay->setText("图片加载失败");
//             }
//         } else {
//             ui->labelImageDisplay->setText("图片文件不存在");
//         }
//     } else {
//         ui->labelImageDisplay->setText("无图片");
//     }
// }

void MainWindow::handleZoomIn()
{
    imageZoomFactor *= 1.2;
    refreshCurrentImages();
    // QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    // if (currentItem) {
    //     int id = currentItem->data(Qt::UserRole).toInt();
    //     if (knowledgePoints.contains(id)) {
    //         displayImage(knowledgePoints[id].imagePath);
    //     }
    // }
}

void MainWindow::handleZoomOut()
{
    imageZoomFactor /= 1.2;
    if (imageZoomFactor < 0.1) imageZoomFactor = 0.1;
    refreshCurrentImages();

    // QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    // if (currentItem) {
    //     int id = currentItem->data(Qt::UserRole).toInt();
    //     if (knowledgePoints.contains(id)) {
    //         displayImage(knowledgePoints[id].imagePath);
    //     }
    // }
}

void MainWindow::handleResetZoom()
{
    imageZoomFactor = 1.0;
    refreshCurrentImages();
//     QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
//     if (currentItem) {
//         int id = currentItem->data(Qt::UserRole).toInt();
//         if (knowledgePoints.contains(id)) {
//             displayImage(knowledgePoints[id].imagePath);
//         }
//     }
}
//图片储存
QString MainWindow::getImageStoragePath()
{
    // 使用应用程序所在目录下的 images 文件夹
    QString appDir = QCoreApplication::applicationDirPath();
    QString imageDirPath = QDir(appDir).filePath("images");

    qDebug() << "应用程序目录:" << appDir;
    qDebug() << "图片存储目录:" << imageDirPath;

    // 确保目录存在
    QDir imageDir(imageDirPath);
    if (!imageDir.exists()) {
        imageDir.mkpath(".");
    }

    return imageDirPath;
}


//目录创建
bool MainWindow::ensureImageStorageDirectory()
{
    QDir imageDir(m_imageStoragePath); //构造QDIR就可以对该目录做遍历等各种操作
    if (!imageDir.exists()) {
        return imageDir.mkpath(".");  //一次性创建一系列不存在目录
    }
    return true;
}

//复制文件函数
QString MainWindow::copyImageToStorage(const QString &sourceImagePath)
{
    if (sourceImagePath.isEmpty()) {
        return "";
    }

    QFileInfo sourceFileInfo(sourceImagePath);  //专门输出文件的各种信息，大小，存在，类型等
    if (!sourceFileInfo.exists() || !sourceFileInfo.isFile()) {
        qDebug() << "Source image file does not exist:" << sourceImagePath;
        return "";
    }

    // 确保存储目录存在
    if (!ensureImageStorageDirectory()) {
        qDebug() << "Failed to create image storage directory";
        return sourceImagePath; // 返回原路径作为备用
    }

    // 生成唯一的文件名（使用时间戳+随机数避免重名）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    int randomNum = QRandomGenerator::global()->bounded(1000, 9999);
    QString baseName = QString("%1_%2").arg(timestamp).arg(randomNum);

    // 保持原文件扩展名
    QString extension = sourceFileInfo.suffix();
    if (extension.isEmpty()) {
        extension = "png"; // 默认扩展名
    }

    QString targetFileName = baseName + "." + extension;
    QString targetFilePath = QDir(m_imageStoragePath).filePath(targetFileName);  //专有拼凑语言，将合成文件名放入打算复制的路径

    // 复制文件
    if (QFile::copy(sourceImagePath, targetFilePath)) {
        qDebug() << "Image copied to:" << targetFilePath;
        return targetFilePath;
    } else {
        qDebug() << "Failed to copy image from" << sourceImagePath << "to" << targetFilePath;
        return sourceImagePath; // 复制失败，返回原路径
    }
}
//以上

//图片放大
// bool MainWindow::eventFilter(QObject *watched, QEvent *event)
// {
//     if (watched == ui->labelImageDisplay && event->type() == QEvent::MouseButtonPress) {
//         QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
//         if (mouseEvent->button() == Qt::LeftButton) {
//             handleImageClicked();
//             return true;
//         }
//     }
//     return QMainWindow::eventFilter(watched, event);
// }

//多图片放大
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            // 判断是否是图片 QLabel 被点击
            QLabel *label = qobject_cast<QLabel*>(watched);
            if (label && label->property("imagePath").isValid()) {
                QString imgPath = label->property("imagePath").toString();
                handleImageClicked(imgPath);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::handleImageClicked(const QString &imagePath)
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return;

    int id = currentItem->data(Qt::UserRole).toInt();
    if (!knowledgePoints.contains(id)) return;

    const KnowledgePoint &point = knowledgePoints[id];

    // 优先使用文件路径
    // if (!point.imagePath.isEmpty() && QFile::exists(point.imagePath)) {
    //     m_imageViewer->setImage(point.imagePath);
    //     m_imageViewer->exec();
    //     return;
    // }
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        m_imageViewer->setImage(imagePath);
        m_imageViewer->exec();
    }

    // 安全的方式：检查是否有图片显示
    // if (ui->labelImageDisplay->pixmap() != nullptr) {
    //     const QPixmap *pixmap = ui->labelImageDisplay->pixmap();
    //     if (!pixmap->isNull()) {
    //         m_imageViewer->setImage(*pixmap);
    //         m_imageViewer->exec();
    //     }
    // }
}

void MainWindow::on_familiarButton_clicked()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return;
    int id = currentItem->data(Qt::UserRole).toInt();
    int reviewvalue=10;
    markAsReviewed(id,reviewvalue);
    if (m_isReviewMode) {
        nextReviewItem();
    }
}


void MainWindow::on_indistinctButton_clicked()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return;
    int id = currentItem->data(Qt::UserRole).toInt();
    int reviewvalue=-5;
    markAsReviewed(id,reviewvalue);
    if (m_isReviewMode) {
        nextReviewItem();
    }
}


void MainWindow::on_forgetButton_clicked()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return; //防止空指针
    int id = currentItem->data(Qt::UserRole).toInt();
    int reviewvalue=-10;
    markAsReviewed(id,reviewvalue);
    if (m_isReviewMode) {
        nextReviewItem();
    }
}


void MainWindow::on_showcontent_clicked()
{
    showContentInTextEdit();
}

void MainWindow::hideContentInTextEdit()
{
    // 设置文字颜色与背景同色（假设背景是白色）
    // ui->textContent->setStyleSheet("QPlainTextEdit { color: white; background-color: white; }");
    ui->textContent->setStyleSheet("color: transparent;");
    // 或者仅修改文字颜色，背景保留原样： color: transparent; 也可以用
    // ui->textContent->setStyleSheet("QPlainTextEdit { color: transparent; }");
}

void MainWindow::showContentInTextEdit()
{
    // 恢复为正常黑色文字，背景保持原样
    // ui->textContent->setStyleSheet("QPlainTextEdit { color: black; }");
    ui->textContent->setStyleSheet("");
}


// 启动每日复习：收集当天需复习且未掌握的知识点
void MainWindow::startDailyReview()
{
    QDate today = QDate::currentDate();
    m_reviewQueue.clear();

    // 按下次复习时间的升序收集（越早越优先）
    QList<KnowledgePoint> points = knowledgePoints.values();
    std::sort(points.begin(), points.end(),
              [](const KnowledgePoint &a, const KnowledgePoint &b) {
                  return a.nextReviewDate < b.nextReviewDate;
              });
    if (!m_reviewQueue.isEmpty()) {
        QMessageBox::information(this, "开始今日复习！", "今天开始复习。");
        return;
    }

    for (const auto &point : points) {
        if (point.nextReviewDate <= today && point.status != STATUS_MASTERED) {
            m_reviewQueue.append(point.id);
        }
    }

    if (m_reviewQueue.isEmpty()) {
        ui->textquestion->setPlainText("今日没有需要复习的知识点，你可以自由学习。");
        return;
    }

    m_isReviewMode = true;
    m_currentReviewIndex = 0;
    showCurrentReviewItem();
    handleCalendarClicked(QDate::currentDate());
}

// 显示当前复习队列中的知识点
void MainWindow::showCurrentReviewItem()
{
    if (m_currentReviewIndex < 0 || m_currentReviewIndex >= m_reviewQueue.size()) {
        endReviewMode();
        return;
    }

    int id = m_reviewQueue[m_currentReviewIndex];

    // 在列表中同步选中该项（屏蔽信号避免触发选择变更处理）
    bool blocked = ui->listKnowledgePoints->blockSignals(true);
    for (int i = 0; i < ui->listKnowledgePoints->count(); ++i) {
        QListWidgetItem *item = ui->listKnowledgePoints->item(i);
        if (item->data(Qt::UserRole).toInt() == id) {
            ui->listKnowledgePoints->setCurrentItem(item);
            break;
        }
    }
    ui->listKnowledgePoints->blockSignals(blocked);

    // 显示详细内容（包括标题、图片、状态等）
    showKnowledgePointDetails(id);
}

// 切换到下一个复习项
void MainWindow::nextReviewItem()
{
    m_currentReviewIndex++;
    if (m_currentReviewIndex >= m_reviewQueue.size()) {
        endReviewMode();
    } else {
        showCurrentReviewItem();
    }
}

// 结束复习模式
void MainWindow::endReviewMode()
{
    m_isReviewMode = false;
    m_reviewQueue.clear();
    m_currentReviewIndex = -1;
    ui->textquestion->setPlainText("今日复习已完成！");
    // 可选：弹出提示
    QMessageBox::information(this, "复习完成", "恭喜！今日复习任务已全部完成。");
}

//批量导入功能
void MainWindow::on_btnImportData_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择要导入的 JSON 文件",
                                                    "",
                                                    "JSON 文件 (*.json)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件！请检查文件权限或路径。");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "JSON 格式错误",
                             QString("无法解析为 JSON 文件：\n%1\n\n错误位置：偏移 %2")
                                 .arg(parseError.errorString())
                                 .arg(parseError.offset));
        return;
    }

    if (!doc.isArray()) {
        QMessageBox::warning(this, "格式错误",
                             "JSON 文件的最外层必须是一个数组（以 [ 开头，以 ] 结尾）。");
        return;
    }

    QJsonArray array = doc.array();
    int importedCount = 0;
    int skippedCount = 0;

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();

        // 检查必要字段：必须包含 title 且非空
        if (!obj.contains("title") || obj["title"].toString().trimmed().isEmpty()) {
            skippedCount++;
            continue;
        }

        KnowledgePoint point;
        point.id = nextId++;  // 自动分配 ID
        point.title = obj["title"].toString().trimmed();
        point.content = obj.contains("content") ? obj["content"].toString() : "";
        // 处理多图片导入（兼容旧格式 imagePath 和 新格式 imagePaths）
        if (obj.contains("imagePath") && obj["imagePath"].isString()) {
            QString singlePath = obj["imagePath"].toString().trimmed();
            if (!singlePath.isEmpty()) {
                // 如果是本地存在的文件，复制到存储目录
                if (QFile::exists(singlePath)) {
                    point.imagePaths.append(copyImageToStorage(singlePath));
                } else {
                    // 可能已经是存储路径，直接添加
                    point.imagePaths.append(singlePath);
                }
            }
        }
        if (obj.contains("imagePaths") && obj["imagePaths"].isArray()) {
            QJsonArray arr = obj["imagePaths"].toArray();
            for (const QJsonValue &val : arr) {
                QString path = val.toString().trimmed();
                if (!path.isEmpty()) {
                    if (QFile::exists(path)) {
                        point.imagePaths.append(copyImageToStorage(path));
                    } else {
                        point.imagePaths.append(path);
                    }
                }
            }
        }
        // point.imagePaths = obj.contains("imagePath") ? obj["imagePath"].toString() : "";
        point.category = obj.contains("category") ? obj["category"].toString() : "未分类";
        point.status = STATUS_NEW;
        point.masteryLevel = 0;
        point.memoryStability = 1.0;
        point.createDate = QDate::currentDate();
        point.lastReviewDate = QDate();  // 无效日期，表示从未复习
        point.nextReviewDate = QDate::currentDate().addDays(1);
        point.reviewCount = 0;

        // 若提供了本地图片路径且存在，则复制到统一图片存储目录
        // if (!point.imagePaths.isEmpty() && QFile::exists(point.imagePaths)) {
        //     point.imagePaths = copyImageToStorage(point.imagePaths);
        // }

        knowledgePoints[point.id] = point;
        importedCount++;
    }

    if (importedCount == 0) {
        QMessageBox::information(this, "导入结果",
                                 "没有成功导入任何知识点。请检查JSON格式（需包含 title 字段且不为空）。");
    } else {
        saveKnowledgePoints();
        refreshKnowledgeList();
        updateStatistics();

        QString msg = QString("成功导入 %1 条知识点。").arg(importedCount);
        if (skippedCount > 0) {
            msg += QString("\n跳过 %1 条（缺少标题或标题为空）。").arg(skippedCount);
        }
        QMessageBox::information(this, "导入完成", msg);
    }
}
//多图片展示
void MainWindow::displayImages(const QStringList &imagePaths)
{
    // 获取布局并清空
    // QHBoxLayout *layout = qobject_cast<QHBoxLayout*>(ui->scrollContent->layout());
    // if (!layout) return;
    QHBoxLayout *layout = qobject_cast<QHBoxLayout*>(ui->scrollContent->layout());
    if (!layout) {
        layout = new QHBoxLayout(ui->scrollContent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        ui->scrollContent->setLayout(layout);
    }
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->removeEventFilter(this);
            delete child->widget();
        }
        delete child;
    }

    if (imagePaths.isEmpty()) {
        QLabel *placeholder = new QLabel("无图片");
        placeholder->setAlignment(Qt::AlignCenter);
        layout->addWidget(placeholder);
        return;
    }

    for (const QString &path : imagePaths) {
        QLabel *imgLabel = new QLabel;
        imgLabel->setMinimumSize(200, 150);
        imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        imgLabel->setAlignment(Qt::AlignCenter);

        if (!path.isEmpty()) {
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                QSize scaledSize = pixmap.size() * imageZoomFactor;
                QPixmap scaled = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imgLabel->setPixmap(scaled);
                imgLabel->setProperty("imagePath", path);
            } else {
                imgLabel->setText("图片加载失败");
            }
        } else {
            imgLabel->setText("无图片");
        }

        imgLabel->installEventFilter(this);
        imgLabel->setCursor(Qt::PointingHandCursor);
        layout->addWidget(imgLabel);
    }
}
// void MainWindow::displayImages(const QStringList &imagePaths) {
//     // 清空旧内容
//     QLayoutItem *child;
//     while ((child = ui->scrollContent->layout()->takeAt(0)) != nullptr) {
//         delete child->widget();
//         delete child;
//     }

//     if (imagePaths.isEmpty()) {
//         QLabel *placeholder = new QLabel("无图片");
//         placeholder->setAlignment(Qt::AlignCenter);
//         ui->scrollContent->layout()->addWidget(placeholder);
//         return;
//     }

//     for (const QString &path : imagePaths) {
//         QLabel *imgLabel = new QLabel;
//         imgLabel->setMinimumSize(200, 150);
//         imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//         imgLabel->setAlignment(Qt::AlignCenter);

//         if (!path.isEmpty()) {
//             QPixmap pixmap(path);
//             if (!pixmap.isNull()) {
//                 QSize scaledSize = pixmap.size() * imageZoomFactor;
//                 QPixmap scaled = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
//                 imgLabel->setPixmap(scaled);
//             } else {
//                 imgLabel->setText("图片加载失败");
//             }
//         } else {
//             imgLabel->setText("无图片");
//         }
//         ui->scrollContent->layout()->addWidget(imgLabel);
//     }
// }
//缩放函数
void MainWindow::refreshCurrentImages()
{
    QListWidgetItem *currentItem = ui->listKnowledgePoints->currentItem();
    if (!currentItem) return;

    int id = currentItem->data(Qt::UserRole).toInt();
    if (knowledgePoints.contains(id)) {
        displayImages(knowledgePoints[id].imagePaths);
    }
}
