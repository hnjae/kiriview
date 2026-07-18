class QQuickItem;

#include "imagesequence_p.h"
#include "imagesequencesource_p.h"
#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportstate_p.h"
#include "imageviewporttoken_p.h"
#include "imageviewportvalidation_p.h"
#include "internalobservation_p.h"
#include "presentationgeometry_p.h"
#include "renderadapter_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportproviderbridge_p.h"
#include "viewportprovidercontract_p.h"
#include "viewportproviderevent_p.h"
#include "viewportrendercontract_p.h"

#include <type_traits>

template <typename Type, typename = void> struct IsComplete : std::false_type
{
};

template <typename Type>
struct IsComplete<Type, std::void_t<decltype(sizeof(Type))>> : std::true_type
{
};

static_assert(!IsComplete<QQuickItem>::value);
