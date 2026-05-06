#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QDate>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QMouseEvent>
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 知识点状态枚举
enum KnowledgeStatus {
    STATUS_NEW,         // 新知识点
    STATUS_LEARNING,    // 学习中
    STATUS_REVIEWING,   // 复习中
    STATUS_MASTERED     // 已掌握
};

// 知识点数据结构
struct KnowledgePoint {
    int id;
    QString title;
    QString content;
    QStringList imagePaths;//更新多图片
    QString category;
    KnowledgeStatus status;
    int masteryLevel; // 掌握程度 0-100
    QDate createDate;
    QDate lastReviewDate;
    QDate nextReviewDate;
    int reviewCount;
    int reviewtureCount;
    double memoryStability = 1.0; // S, 记忆稳定性，初始值
};

// 前向声明
class ImageViewerDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT



public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 按钮点击槽函数
    void handleAddNew();
    void handleEditPoint();
    void handleMarkReviewed();
    void handleDeletePoint();
    void handleExportData();
    void handleClearSearch();

    // 其他交互槽函数
    void handleListSelectionChanged();
    void handleListItemDoubleClicked(QListWidgetItem *item);
    void handleSearchTextChanged(const QString &text);
    void handleFilterCategoryChanged(int index);
    void handleFilterStatusChanged(int index);
    void handleStatusChanged(int index);
    void handleCalendarClicked(const QDate &date);

    // 图片操作槽函数
    void handleZoomIn();
    void handleZoomOut();
    void handleResetZoom();
    //处理图片点击
    void handleImageClicked(const QString &imagePath);

    void on_familiarButton_clicked();

    void on_indistinctButton_clicked();

    void on_forgetButton_clicked();
    //处理备份
    // 获取备份文件夹路径，并确保目录存在
    QString getBackupPath() const;
    // 删除7天前的旧备份文件
    void cleanOldBackups() const;

    void on_showcontent_clicked();
    //隐藏文字，点击可看
    void hideContentInTextEdit();   // 将内容文字颜色设为白色（隐藏）
    void showContentInTextEdit();   // 将内容文字颜色设为黑色（显示）

    void on_btnImportData_clicked();

private:
    Ui::MainWindow *ui;
    QMap<int, KnowledgePoint> knowledgePoints;
    int nextId = 1;
    double imageZoomFactor = 1.0; // 图片缩放因子

    QString m_imageStoragePath; // 图片存储路径

    bool m_isRefreshing = false;// 防止刷新递归

    // 记忆曲线间隔（天数）
    const QVector<int> reviewIntervals = {1, 2, 4, 7, 15, 30, 60, 90};

    void loadKnowledgePoints();
    void saveKnowledgePoints();
    void refreshKnowledgeList();
    void updateStatistics();
    void showKnowledgePointDetails(int id);
    void addKnowledgePoint(const QString &title, const QString &content,
                           const QStringList &imagePath, const QString &category);
    void editKnowledgePoint(int id, const QString &title, const QString &content,
                            const QStringList &imagePath, const QString &category);
    void markAsReviewed(int id,int reviewvalue);
    // QDate calculateNextReviewDate(int currentLevel, int reviewCount); //旧
    QDate calculateNextReviewDate(int id);//新
    void updateMasteryLevel(int id, int newLevel);
    void filterKnowledgePoints();
    void displayImage(const QString &imagePath); // 显示图片函数

    // 当前过滤条件
    QString currentSearchText;
    QString currentCategoryFilter;
    QString currentStatusFilter;

    //图片储存函数
    QString copyImageToStorage(const QString &sourceImagePath);
    QString getImageStoragePath();
    bool ensureImageStorageDirectory();

    ImageViewerDialog *m_imageViewer; // 图片查看对话框

    // void debugDataSources();//看资源在哪的，可删
    QDate m_lastBackupDate;//7天备份

    //自动弹出复习
    QList<int> m_reviewQueue;        // 今日待复习的知识点ID列表
    int m_currentReviewIndex = -1;   // 当前复习到的索引
    bool m_isReviewMode = false;     //
    void startDailyReview();         // 启动每日复习
    void showCurrentReviewItem();   // 显示当前复习项
    void nextReviewItem();          // 切换到下一项
    void endReviewMode();           // 结束复习模式
    //自适应艾宾浩斯曲线参数更新算法
    void updateMemoryStability(int id, const QString& rating);
    //多图片展示
    void displayImages(const QStringList &imagePaths);
    //缩放函数
    void refreshCurrentImages();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // MAINWINDOW_H
