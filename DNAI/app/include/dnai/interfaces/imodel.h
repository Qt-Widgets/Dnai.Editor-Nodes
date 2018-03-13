#ifndef DNAI_INTERFACES_IMODEL_H
#define DNAI_INTERFACES_IMODEL_H

#include "iserializable.h"
#include "dnai/models/generictreeitem.h"

namespace dnai {
    namespace interfaces {
	    /**
		 * \brief This class allow you to create a new serializable model and allow you to use it in a tree
		 * \tparam T 
		 */
		template <class T>
        class IModel: public models::GenericTreeItem, public ASerializable<T>
        {
		public:
            explicit IModel(GenericTreeItem *parent = nullptr) : models::GenericTreeItem(parent) {}
        };
    }
}

#endif // DNAI_INTERFACES_IMODEL_H
