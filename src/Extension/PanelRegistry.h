#ifndef PANELREGISTRY_H
#define PANELREGISTRY_H

#include "ExtensionInterface.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

class QLibrary;

// =============================================================================
// 面板注册处
//
// 【和 CameraFactory 是同一个模式】
//   CameraFactory 用 QMap<厂商名, 创建器> 收口「有哪些品牌」；
//   这里用 QMap<面板 id, 创建器> 收口「有哪些面板」。
//   两处都只做登记与创建，不认识具体类型。
//
// 【为什么用创建器（lambda）而不是存指针】
//   面板要能反复开关：关掉再打开应该是一个干净的新实例。
//   存「怎么造」比存「造好的那一个」灵活，也不会出现两个宿主抢同一个实例。
// =============================================================================

class PanelRegistry {
public:
    using PanelCreator = std::function<IPanel*()>;

    static PanelRegistry* Instance();

    // 登记一个面板创建器。同 id 重复登记会被拒绝（返回 false），
    // 避免插件把内置面板顶掉。
    bool Register(const QString& id, PanelCreator creator);

    bool Contains(const QString& id) const;

    // 从外部 DLL 加载面板，返回成功登记的数量。
    // DLL 需按 ExtensionInterface.h 的约定导出两个 C 函数。
    int LoadFromLibrary(const QString& libraryPath);

    // 按登记顺序返回全部面板 id
    QStringList Ids() const;

    // 造一个面板实例。id 未登记时返回 nullptr，调用方负责释放
    IPanel* Create(const QString& id) const;

private:
    PanelRegistry() { }
    ~PanelRegistry() { }

    // 用「列表 + 线性查找」而不是 QMap：面板数量很少（个位数），
    // 换来的是登记顺序稳定 —— 侧边栏和菜单里的排列顺序由注册顺序决定，
    // 用 QMap 会按 id 字母序排，顺序就不受控了。
    struct Entry {
        QString id;
        PanelCreator creator;
    };
    QList<Entry> m_panels;

    // 已加载的扩展库必须一直活着：
    // 面板对象的虚函数表指向库里的代码，库一旦卸载，面板调用就会跳到野地址。
    // 所以这里故意持有且不释放，进程退出时由系统回收。
    QList<QLibrary*> m_libraries;
};

#endif
