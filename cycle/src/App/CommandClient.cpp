#include "CommandClient.h"

CommandClient::CommandClient(SingletonRepo* repo) :
        SingletonAccessor(repo, "CommandClient") {
}
