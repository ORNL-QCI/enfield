#include "enfield/Support/Graph.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

// ----------------------------- Graph -------------------------------
efd::Graph::Graph(unsigned n) : mN(n) {
    mSuccessors.assign(n, std::set<unsigned>());
    mPredecessors.assign(n, std::set<unsigned>());
    mId.assign(n, std::string());
}

unsigned efd::Graph::inDegree(unsigned i) const {
    return mPredecessors[i].size();
}

unsigned efd::Graph::outDegree(unsigned i) const {
    return mSuccessors[i].size();
}

unsigned efd::Graph::size() const {
    return mN;
}

std::set<unsigned>& efd::Graph::succ(unsigned i) {
    return mSuccessors[i];
}

std::set<unsigned>& efd::Graph::pred(unsigned i) {
    return mPredecessors[i];
}

bool efd::Graph::hasEdge(unsigned i, unsigned j) {
    std::set<unsigned>& succ = this->succ(i);
    return succ.find(j) != succ.end();
}

void efd::Graph::putEdge(unsigned i, unsigned j) {
    succ(i).insert(j);
    pred(j).insert(i);
}

unsigned efd::Graph::putVertex(std::string s) {
    if (mStrToId.find(s) != mStrToId.end())
        return mStrToId[s];
    unsigned idx = mStrToId.size();
    mStrToId[s] = idx;
    return idx;
}

bool efd::Graph::isReverseEdge(unsigned i, unsigned j) {
    return mReverseEdges.find(std::pair<unsigned, unsigned>(i, j)) != mReverseEdges.end();
}

unsigned efd::Graph::getUId(std::string s) {
    assert(mStrToId.find(s) != mStrToId.end() && "String Id not found.");
    return mStrToId[s];
}

std::string efd::Graph::getSId(unsigned i) {
    assert(mId.size() > i && "Index out of bounds.");
    return mId[i];
}

void efd::Graph::buildReverseGraph() {
    for (unsigned i = 0; i < mN; ++i) {
        std::set<unsigned>& succ = this->succ(i);

        for (unsigned k : succ) {
            if (!hasEdge(k, i)) {
                putEdge(k, i);
                mReverseEdges.insert(std::pair<unsigned, unsigned>(k, i));
            }
        }
    }
}

std::unique_ptr<efd::Graph> efd::Graph::Create(unsigned n) {
    return std::unique_ptr<Graph>(new Graph(n));
}

static std::unique_ptr<efd::Graph> ReadFromIn(std::istream& in) {
    unsigned n;
    in >> n;

    std::unique_ptr<efd::Graph> graph(efd::Graph::Create(n));
    for (std::string uS, vS; in >> uS >> vS;) {
        unsigned u = graph->putVertex(uS);
        unsigned v = graph->putVertex(vS);
        graph->putEdge(u, v);
    }

    graph->buildReverseGraph();
    return graph;
}

std::unique_ptr<efd::Graph> efd::Graph::Read(std::string filepath) {
    std::ifstream in(filepath.c_str());
    return ReadFromIn(in);
}

std::unique_ptr<efd::Graph> efd::Graph::ReadString(std::string graphStr) {
    std::stringstream in(graphStr);
    return ReadFromIn(in);
}