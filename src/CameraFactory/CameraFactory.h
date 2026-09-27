#ifndef CAMERAFACTORY_H
#define CAMERAFACTORY_H

#include "../CameraInterface/CameraInterface.h"
#include <QMap>
#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>

class CameraFactory {
public:

    // 进程级单例：注册表在首次 instance() 时一次性装配，之后只读访问。
    // 故意不留释放路径——静态析构顺序不可控，交给进程退出回收更安全。
    static CameraFactory* instance();

    // 接新品牌只动这一处：按同一个 T 同时登记创建器与枚举器，缺一即半残——
    // 只登记创建器则枚举不出设备，只登记枚举器则创建必然失败。
    // 代价是 T 需满足两个硬约束：能从 CameraMetaInfo 构造、有 static EnumCamera(QVector<CameraMetaInfo>&)。
    // 登记一个品牌的创建器与枚举器
    template <typename T>
    void registerVendor(const QString& venderName)
    {
        m_creatorMap[venderName] = [](const CameraMetaInfo& info) -> CameraInterface* {
            return new T(info);
        };
        m_enumeratorMap[venderName] = [](QVector<CameraMetaInfo>& cameraInfos) -> uint32_t {
            return T::EnumCamera(cameraInfos);
        };
    }

    // 未注册品牌回 nullptr 而非抛异常，调用方必须判空；返回实例所有权归调用方，由它 delete。
    CameraInterface* createCamera(const CameraMetaInfo& info);

    // 依次调用各品牌的枚举器，结果追加到 cameraInfos
    // 不做跨品牌去重，同机被两品牌枚举出由调用方过滤；各枚举器的错误被丢弃，本函数恒成功。
    uint32_t enumCameras(QVector<CameraMetaInfo>& cameraInfos) const;

    QStringList getSupportedVenders() const;

    // 品牌名必须与 EnumCamera 写进 CameraMetaInfo.VenderName 的串逐字一致；
    // 海康链路里该值取自设备的厂商字段而非本地常量，匹配格外脆弱。
    bool isVenderSupported(const QString& venderName) const;

private:
    CameraFactory() = default;
    ~CameraFactory() = default;

    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;

private:
    using CameraCreator = std::function<CameraInterface*(const CameraMetaInfo&)>;
    using CameraEnumerator = std::function<uint32_t(QVector<CameraMetaInfo>&)>;
    QMap<QString, CameraCreator> m_creatorMap;
    QMap<QString, CameraEnumerator> m_enumeratorMap;
    static CameraFactory* m_instance;
    static QMutex m_mutex;
};

#endif

