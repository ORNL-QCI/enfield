#include "cppmicroservices/BundleActivator.h"
#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/ServiceProperties.h"
#include "EnfieldPlacement.hpp"

using namespace cppmicroservices;

class US_ABI_LOCAL Enfield_Activator : public BundleActivator {
public:
  Enfield_Activator() {}

  void Start(BundleContext context) 
  {
    // TODO
  }

  void Stop(BundleContext context) {}
};

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(Enfield_Activator)
