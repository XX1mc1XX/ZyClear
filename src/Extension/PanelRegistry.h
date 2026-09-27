#ifndef PANELREGISTRY_H
#define PANELREGISTRY_H

#include "ExtensionInterface.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

class QLibrary;

class PanelRegistry {
public:
    using PanelCreator = std::function<IPanel*()>;

    // 登记表必须全程序唯一：主窗口、控制台模式、外部插件都往同一张表写，
    // 各自持一份副本就看不见彼此登记的面板了。
    static PanelRegistry* Instance();

    // 同 id 重复登记会被拒绝，避免插件把内置面板顶掉
    bool Register(const QString& id, PanelCreator creator);

    bool Contains(const QString& id) const;

    // 返回本库成功登记的面板数。库能加载但缺导出符号、或面板 id 与已有冲突，
    // 都只算这个库少登记几个，不视为失败 —— 加载异常只记日志，不往上抛。
    int LoadFromLibrary(const QString& libraryPath);

    // 按登记顺序返回全部面板 id
    QStringList Ids() const;

    // id 未登记时返回 nullptr，调用方负责释放
    IPanel* Create(const QString& id) const;

private:
    PanelRegistry() { }
    ~PanelRegistry() { }

    // 用列表 + 线性查找而不是 QMap：面板只有个位数，图的是登记顺序稳定，
    // 用 QMap 会按 id 字母序排，侧边栏顺序就不受控了。
    struct Entry {
        QString id;
        PanelCreator creator;
    };
    QList<Entry> m_panels;

    // 故意持有不释放：面板的虚函数表指向库里的代码，库一卸载调用就跳到野地址
    QList<QLibrary*> m_libraries;
};

#endif
