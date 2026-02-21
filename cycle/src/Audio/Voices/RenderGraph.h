#ifndef AUDIO_VOICES_RENDERGRAPH_H_
#define AUDIO_VOICES_RENDERGRAPH_H_

#include <vector>
using std::vector;

class RenderGraph
{
public:
    RenderGraph();
    virtual ~RenderGraph();

    void addColumn();
    void addRow();
    void resize(int rows, int cols);

private:
    vector<vector<int> > mappings;
};

#endif
