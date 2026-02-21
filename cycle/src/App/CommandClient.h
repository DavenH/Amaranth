#pragma once

#include <App/SingletonAccessor.h>

class CommandClient :
        public SingletonAccessor
    ,	public ApplicationCommandManager {
public:
    CommandClient(SingletonRepo* repo);
    virtual ~CommandClient() = default;
};