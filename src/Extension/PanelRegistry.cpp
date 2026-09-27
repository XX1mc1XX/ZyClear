#include "PanelRegistry.h"

#include "ExtensionInterface.h"

#include <QDebug>
#include <QLibrary>

PanelRegistry* PanelRegistry::Instance()
{
    static PanelRegistry instance;
    return &instance;
}

bool PanelRegistry::Register(const QString& id, PanelCreator creator)
{
    if (id.isEmpty() || !creator) {
        return false;
    }

    if (Contains(id)) {
        qWarning() << "面板 id 已存在，忽略重复登记:" << id;
        return false;
    }

    m_panels.append(Entry { id, std::move(creator) });
    return true;
}

bool PanelRegistry::Contains(const QString& id) const
{
    for (const Entry& entry : m_panels) {
        if (entry.id == id) {
            return true;
        }
    }
    return false;
}

int PanelRegistry::LoadFromLibrary(const QString& libraryPath)
{
    auto* library = new QLibrary(libraryPath);
    if (!library->load()) {
        qWarning() << "加载扩展库失败:" << libraryPath << library->errorString();
        delete library;
        return 0;
    }

    using CountFunction = int (*)();
    using AtFunction = IPanel* (*)(int);

    auto countFunction = reinterpret_cast<CountFunction>(library->resolve(ZYCLEAR_EXTENSION_COUNT_FN));
    auto atFunction = reinterpret_cast<AtFunction>(library->resolve(ZYCLEAR_EXTENSION_AT_FN));

    if (countFunction == nullptr || atFunction == nullptr) {
        qWarning() << "扩展库缺少约定的导出函数:" << libraryPath;
        delete library;
        return 0;
    }

    m_libraries.append(library);

    int loaded = 0;
    const int count = countFunction();
    for (int index = 0; index < count; ++index) {
        // 先造一个实例只为读出 id，读完即弃
        IPanel* probe = atFunction(index);
        if (probe == nullptr) {
            continue;
        }
        const QString id = probe->PanelId();
        delete probe;

        if (id.isEmpty()) {
            continue;
        }

        // 转发到插件入口，每次调用返回新实例
        if (Register(id, [atFunction, index]() { return atFunction(index); })) {
            ++loaded;
        }
    }

    qInfo() << "扩展库加载完成:" << libraryPath << "面板数:" << loaded;
    return loaded;
}

QStringList PanelRegistry::Ids() const
{
    QStringList ids;
    for (const Entry& entry : m_panels) {
        ids << entry.id;
    }
    return ids;
}

IPanel* PanelRegistry::Create(const QString& id) const
{
    for (const Entry& entry : m_panels) {
        if (entry.id == id) {
            return entry.creator();
        }
    }
    return nullptr;
}
