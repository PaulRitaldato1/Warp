#include <UI/ComponentDescriptor.h>

HashMap<u32, ComponentDescriptor>& GetComponentDescriptors()
{
	static HashMap<u32, ComponentDescriptor> descriptors;
	return descriptors;
}
