#ifndef BUILTINPANELS_H
#define BUILTINPANELS_H

#include "Ai/ToolProviderInterface.h"

#include <QList>

class PanelRegistry;

void RegisterBuiltinPanels(PanelRegistry* registry, const QList<IToolProvider*>& providers);

#endif
