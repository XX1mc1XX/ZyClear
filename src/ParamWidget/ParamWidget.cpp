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
    m_pParamDescript->setFixedHeight(58);

    QStringList headerList;
    headerList << "Param" << "Value";
    m_pModel = new CameraParamModel(headerList);
    m_pParamTreeView->setModel(m_pModel);
    m_pParamTreeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_pParamTreeView->header()->setDefaultSectionSize(150);
    m_pParamTreeView->setItemDelegate(m_pCameraParamDelegate);
    m_pParamTreeView->expandAll();
    m_pSelectionModel = m_pParamTreeView->selectionModel();

    m_pParamDescript->append(QStringLiteral("相机参数注释"));

    connect(m_pSelectionModel, &QItemSelectionModel::selectionChanged,
        this, &ParamWidget::OnUpdataSelection);
    connect(m_pModel, &CameraParamModel::SigValueChanged, this, &ParamWidget::writeCameraParam);

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
    QString serial = CameraContext::Instance()->currentSerial();

    for (auto param : paramList) {

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
    m_pParamTreeView->reset();
}

void ParamWidget::writeCameraParam(const QModelIndex& index)
{
    QVariant dataValue = m_pModel->data(index, CameraParamModel::ItemRoles::ParamRole);
    CameraParam curCameraParam = dataValue.value<CameraParam>();

    QString serial = CameraContext::Instance()->currentSerial();

    CHECK_RETURN(CameraContext::Instance()->writeParam(serial, curCameraParam));
}

void ParamWidget::RespondMessage(int message)
{
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

    QModelIndex index = selected.indexes().first();
    QVariant varValue = m_pParamTreeView->model()->data(index, CameraParamModel::ParamDescriptionRole);
    QString strDescript = varValue.toString();
    m_pParamDescript->append(strDescript);
}

void ParamWidget::on_Refresh_Button_clicked()
{
    m_pParamTreeView->reset();
    m_pParamTreeView->expandAll();
}

