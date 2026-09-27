#include "PanelRegistry.h"

#include "ExtensionInterface.h"

#include <QDebug>
#include <QLibrary>

PanelRegistry* PanelRegistry::Instance()
{
    // 函数内 static 的初始化由编译器加锁保证只跑一次，无需自己上锁；
    // 且首次真正用到登记表时才构造，避开跨编译单元的全局构造顺序问题。
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

    // resolve 只认符号名、不认类型，返回的是裸地址，强转成函数指针的类型安全
    // 完全靠双方约定：导出端必须真的是这个签名。签名不符编译期查不出来，
    // 只会在真正调用时炸。
    using CountFunction = int (*)();
    using AtFunction = IPanel* (*)(int);

    auto countFunction = reinterpret_cast<CountFunction>(library->resolve(ZYCLEAR_EXTENSION_COUNT_FN));
    auto atFunction = reinterpret_cast<AtFunction>(library->resolve(ZYCLEAR_EXTENSION_AT_FN));

    if (countFunction == nullptr || atFunction == nullptr) {
        qWarning() << "扩展库缺少约定的导出函数:" << libraryPath;
        delete library;
        return 0;
    }

    // 库能解析出两个函数才留下。下面登记的是「转发到 atFunction」的闭包，
    // 真正 new 面板推迟到以后任意一次 Create，所以库必须活到进程结束、不解载。
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
