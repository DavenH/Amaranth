#ifndef COMMANDCLIENT_H_
#define COMMANDCLIENT_H_

#include <App/SingletonAccessor.h>

class CommandClient :
		public SingletonAccessor
	,	public ApplicationCommandManager {
public:
	CommandClient(SingletonRepo* repo);
	virtual ~CommandClient();
};

#endif
