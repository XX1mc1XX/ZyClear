#include "ParamWidget.h"
#include "CameraInterface/ZCCameraMetaInfo.h"
#include "CameraInterface/ZCCameraParam.h"
#include "CameraInterface/CameraContext.h"
#include "CameraInterface/CameraError.h"
#include "ParamWidget/CameraParamDelegate.h"
#include "ParamWidget/CameraParamItem.h"
#include "ParamWidget/CameraParamModel.h"
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSpacerItem>
#include <QSplitter>
#include <QVBoxLayout>

ParamWidget::ParamWidget(QWidget* parent)
    : QWidget(parent)
    , Listener()
    , m_pRefreshButton(nullptr)
    , m_pSplitter(nullptr)
    , m_pParamTreeView(nullptr)
    , m_pParamDescript(nullptr)
    , m_pCameraParamDelegate(new CameraParamDelegate())
{
    setupUi();
    // 定高而不是按内容自适应：说明文字长短悬殊，跟着内容长会顶得参数表上下跳
    m_pParamDescript->setFixedHeight(58);

    QStringList headerList;
    headerList << "Param" << "Value";
    m_pModel = new CameraParamModel(headerList);
    m_pParamTreeView->setModel(m_pModel);
    // 交给用户自己拖列宽：参数名长短在不同型号间差别很大，固定宽度总有一边受委屈
    m_pParamTreeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_pParamTreeView->header()->setDefaultSectionSize(150);
    // 委托接管"值"列的渲染与编辑，按参数类型现场造控件，见 createEditor
    m_pParamTreeView->setItemDelegate(m_pCameraParamDelegate);
    // 此时模型还是空的，这次展开没有实际效果；真正的展开在每次装载后的刷新里
    m_pParamTreeView->expandAll();
    m_pSelectionModel = m_pParamTreeView->selectionModel();

    m_pParamDescript->append(QStringLiteral("相机参数注释"));

    connect(m_pSelectionModel, &QItemSelectionModel::selectionChanged,
        this, &ParamWidget::OnUpdataSelection);
    // 参数区的"改值"与"下发设备"其实是同一个动作：模型一改就发信号，这里立刻写回相机
    connect(m_pModel, &CameraParamModel::SigValueChanged, this, &ParamWidget::writeCameraParam);

    // 只订阅与参数有效性和相机生命周期相关的消息。ListenerManger 既不持有本对象的
    // 所有权、也没有反注册接口，所以本对象必须先于它析构，否则后续通知会打到野指针。
    ListenerManger::Instance()->registerMessage(MESSAGE::CAMERA_CONNECT
            | MESSAGE::CAMERA_DISCONNECT
            | MESSAGE::CAMERA_ENUMRTION
            | MESSAGE::CAMERA_STARTGRAB
            | MESSAGE::CAMERA_STOPTGRAB
            | MESSAGE::CAMERA_CAMERASWICH,
        this);
}

ParamWidget::~ParamWidget()
{
}

void ParamWidget::setupUi()
{
    resize(300, 538);
    setMinimumWidth(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setWindowTitle(tr("Form"));

    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName("verticalLayout");
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* paramFrame = new QFrame(this);
    paramFrame->setObjectName("Param_Frame");
    paramFrame->setFrameShape(QFrame::StyledPanel);
    paramFrame->setFrameShadow(QFrame::Raised);

    QHBoxLayout* horizontalLayout = new QHBoxLayout(paramFrame);
    horizontalLayout->setObjectName("horizontalLayout");
    horizontalLayout->setContentsMargins(5, 5, 5, 5);
    horizontalLayout->addItem(new QSpacerItem(230, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_pRefreshButton = new QPushButton(paramFrame);
    m_pRefreshButton->setObjectName("Refresh_Button");
    m_pRefreshButton->setToolTip(QStringLiteral("刷新相机参数"));
    horizontalLayout->addWidget(m_pRefreshButton);
    verticalLayout->addWidget(paramFrame);

    m_pSplitter = new QSplitter(this);
    m_pSplitter->setObjectName("splitter");
    m_pSplitter->setOrientation(Qt::Vertical);
    m_pSplitter->setOpaqueResize(true);

    QWidget* paramContainer = new QWidget(m_pSplitter);
    paramContainer->setObjectName("Param_widget");
    QVBoxLayout* paramContainerLayout = new QVBoxLayout(paramContainer);
    paramContainerLayout->setObjectName("verticalLayout_2");
    paramContainerLayout->setContentsMargins(0, 0, 0, 0);
    m_pParamTreeView = new QTreeView(paramContainer);
    m_pParamTreeView->setObjectName("Param_treeView");
    m_pParamTreeView->setAlternatingRowColors(true);
    paramContainerLayout->addWidget(m_pParamTreeView);
    m_pSplitter->addWidget(paramContainer);

    m_pParamDescript = new QTextBrowser(m_pSplitter);
    m_pParamDescript->setObjectName("ParamDescript_textBrowser");
    m_pSplitter->addWidget(m_pParamDescript);

    verticalLayout->addWidget(m_pSplitter);

    connect(m_pRefreshButton, &QPushButton::clicked, this, &ParamWidget::on_Refresh_Button_clicked);
}

void ParamWidget::initParamWidget(QVector<CameraParam> paramList)
{
    // 入参按值拷贝：每个 param 都会被 readParam 填成带值和权限位的实体副本，
    // 不碰调用方手里的那份 Schema 模板
    QString serial = CameraContext::Instance()->currentSerial();

    for (auto param : paramList) {

        // 读失败不阻断装载：参数仍以 Schema 给的初值进表，只是把错误抛给上层提示。
        // 三态权限（有效/可读/可写）由 readParam 一并写进 param，只读项的禁编辑即源于此
        auto ret = CameraContext::Instance()->readParam(serial, param);
        if (ret != ZYCLEAR_OK) {
            QString error = param.name() + QString(" read failed");
            emit SigUpdateErrorInfo(error);
        }

        m_pModel->addCameraParam(param);
    }
}

void ParamWidget::clearParamWidget()
{
    m_pModel->clear();
    m_pParamDescript->clear();
    // reset() 不是多余的：模型加/删行都没发 begin/end 通知，只有整树重挂
    // 才能让视图丢掉旧行，把选择与展开状态一并作废
    m_pParamTreeView->reset();
}

void ParamWidget::writeCameraParam(const QModelIndex& index)
{
    // 走到这里时模型已经乐观更新过（setData 先发的信号），所以设备写失败不会回滚
    // 界面：CHECK_RETURN 只把错误抛给 SigUpdateErrorInfo，值就留在界面上。
    // 要改成"设备确认后才改界面"，得让 setData 先写设备、成功再改模型
    QVariant dataValue = m_pModel->data(index, CameraParamModel::ItemRoles::ParamRole);
    CameraParam curCameraParam = dataValue.value<CameraParam>();

    QString serial = CameraContext::Instance()->currentSerial();

    CHECK_RETURN(CameraContext::Instance()->writeParam(serial, curCameraParam));
}

void ParamWidget::RespondMessage(int message)
{
    // 枚举与切换相机都会整体换掉参数集合，先清空，等随后的连接消息重新装载
    if ((message & MESSAGE::CAMERA_ENUMRTION) == MESSAGE::CAMERA_ENUMRTION) {
        clearParamWidget();
    }
    if ((message & MESSAGE::CAMERA_CONNECT) == MESSAGE::CAMERA_CONNECT) {
        this->setEnabled(true);

        QVector<CameraParam> paramList;
        CameraContext::Instance()->getParamList(CameraContext::Instance()->currentSerial(), paramList);
        initParamWidget(paramList);
        on_Refresh_Button_clicked();
    }
    if ((message & MESSAGE::CAMERA_DISCONNECT) == MESSAGE::CAMERA_DISCONNECT) {
        clearParamWidget();
    }
    // 采集期间绝大多数相机锁参数，与其逐项按权限位判断，不如整块禁用省事；停止采集再放行
    if ((message & MESSAGE::CAMERA_STARTGRAB) == MESSAGE::CAMERA_STARTGRAB) {
        this->setEnabled(false);
    }
    if ((message & MESSAGE::CAMERA_STOPTGRAB) == MESSAGE::CAMERA_STOPTGRAB) {
        this->setEnabled(true);
    }
    if ((message & MESSAGE::CAMERA_CAMERASWICH) == MESSAGE::CAMERA_CAMERASWICH) {
        clearParamWidget();
    }
}

void ParamWidget::OnUpdataSelection(const QItemSelection& selected, const QItemSelection& deselected)
{
    m_pParamDescript->clear();

    // 只取第一个索引去查说明：tips 挂在条目上，同一条目两列取到的内容相同。
    // 但模型清空 / reset 会带着空选择走到这里，.first() 就成了越界访问，
    // 所以先判空——"选择变化至少带一个索引"这条视图行为不能当契约使
    const QModelIndexList selectedIndexes = selected.indexes();
    if (selectedIndexes.isEmpty()) {
        return;
    }

    QModelIndex index = selectedIndexes.first();
    QVariant varValue = m_pParamTreeView->model()->data(index, CameraParamModel::ParamDescriptionRole);
    QString strDescript = varValue.toString();
    m_pParamDescript->append(strDescript);
}

void ParamWidget::on_Refresh_Button_clicked()
{
    // 重挂整树并重新展开：参数值可能被相机自己改掉（自动曝光之类），视图必须
    // 重新向模型取值；顺带把展开状态复位，避免某些型号分组默认折叠起来
    m_pParamTreeView->reset();
    m_pParamTreeView->expandAll();
}

