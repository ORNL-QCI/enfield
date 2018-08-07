#include "enfield/Transform/Allocators/BoundedMappingTreeQAllocator.h"
#include "enfield/Transform/Allocators/BMT/DefaultBMTQAllocatorImpl.h"
#include "enfield/Transform/PassCache.h"
#include "enfield/Support/ApproxTSFinder.h"
#include "enfield/Support/CommandLine.h"
#include "enfield/Support/Defs.h"
#include "enfield/Support/Timer.h"

#include <algorithm>
#include <queue>

using namespace efd;
using namespace bmt;

static Opt<uint32_t> MaxChildren
("-bmt-max-children", "Limits the max number of children per partial solution.",
 std::numeric_limits<uint32_t>::max(), false);

static Opt<uint32_t> MaxPartialSolutions
("-bmt-max-partial", "Limits the max number of partial solutions per step.",
 std::numeric_limits<uint32_t>::max(), false);

BoundedMappingTreeQAllocator::BoundedMappingTreeQAllocator(ArchGraph::sRef ag)
    : StdSolutionQAllocator(ag) {}

CandidateVector
BoundedMappingTreeQAllocator::extendCandidates(Dep& dep,
                                               const std::vector<bool>& mapped,
                                               const CandidateVector& candidates,
                                               bool ignoreChildrenLimit) {
    typedef std::pair<uint32_t, uint32_t> Pair;
    typedef std::vector<Pair> PairVector;

    uint32_t a = dep.mFrom, b = dep.mTo;
    uint32_t childrenBound = (ignoreChildrenLimit) ? _undef : mMaxChildren;
    CandidateVector newCandidates;

    for (auto cand : candidates) {
        PairVector pairV;
        CandidateVector localCandidates;
        auto assign = GenAssignment(mPQubits, cand.m, false);

        if (mapped[a] && mapped[b]) {
            uint32_t u = cand.m[a], v = cand.m[b];
            if (mArchGraph->hasEdge(u, v) || mArchGraph->hasEdge(v, u))
                pairV.push_back(Pair(u, v));
        } else if (!mapped[a] && !mapped[b]) {
            for (uint32_t u = 0; u < mPQubits; ++u) {
                if (assign[u] != _undef) continue;
                for (uint32_t v : mArchGraph->adj(u)) {
                    if (assign[v] != _undef) continue;
                    pairV.push_back(Pair(u, v));
                }
            }
        } else {
            uint32_t mappedV;

            if (!mapped[a]) {
                mappedV = b;
            } else {
                mappedV = a;
            }

            uint32_t u = cand.m[mappedV];
            for (uint32_t v : mArchGraph->adj(u)) {
                if (assign[v] == _undef) {
                    if (mappedV == a) {
                        pairV.push_back(Pair(u, v));
                    } else {
                        pairV.push_back(Pair(v, u));
                    }
                }
            }
        }

        for (auto& pair : pairV) {
            auto cpy = cand;
            cpy.m[a] = pair.first;
            cpy.m[b] = pair.second;

            if (!mArchGraph->hasEdge(pair.first, pair.second)) {
                cpy.cost += RevCost.getVal();
            }
            
            localCandidates.push_back(cpy);
        }

        CandidateVector selected = mChildrenCSelector->select(childrenBound, localCandidates);
        newCandidates.insert(newCandidates.end(), selected.begin(), selected.end());
    }

    return mPartialSolutionCSelector->select(mMaxPartial, newCandidates);
}

CandidateVCollection BoundedMappingTreeQAllocator::phase1() {
    // First Phase:
    //     in this phase, we divide the program in layers, such that each layer is satisfied
    //     by any of the mappings inside 'candidates'.
    //
    mPP.push_back(PPartition());
    CandidateVector candidates { { Mapping(mVQubits, _undef), 0 } };
    CandidateVCollection collection;
    std::vector<bool> mapped(mVQubits, false);

    INF << "PHASE 1 >>>> Solving SIP Instances" << std::endl;

    bool first = true;

    while (mNCIterator->hasNext()) {
        auto node = mNCIterator->next();
        auto iDependencies = mDBuilder.getDeps(node);
        auto nofIDeps = iDependencies.getSize();

        if (nofIDeps > 1) {
            ERR << "Instructions with more than one dependency not supported "
                << "(" << iDependencies.mCallPoint->toString(false) << ")" << std::endl;
            ExitWith(ExitCode::EXIT_multi_deps);
        } else if (nofIDeps < 1) {
            continue;
        }

        auto dep = iDependencies[0];
        auto newCandidates = extendCandidates(dep, mapped, candidates, first);
        first = false;

        if (newCandidates.empty()) {
            mPP.push_back(PPartition());
            // Save candidates and reset!
            collection.push_back(candidates);
            // Process this dependency again.
            candidates = { { Mapping(mVQubits, _undef), 0 } };
            mapped.assign(mVQubits, false);
            newCandidates = extendCandidates(dep, mapped, candidates, true);
        }

        mPP.back().push_back(node);

        mapped[dep.mFrom] = true;
        mapped[dep.mTo] = true;
        candidates = newCandidates;

        INF << "Dep (" << dep.mFrom << ", " << dep.mTo << ")" << std::endl;
        INF << "Candidate number: " << candidates.size() << std::endl;
    }

    collection.push_back(candidates);

    return collection;
}

MappingSeq BoundedMappingTreeQAllocator::tracebackPath(const TIMatrix& mem, uint32_t idx) {
    MappingSeq mapSeq;
    uint32_t nofLayers = mem.size();

    mapSeq.mappingCost = mem[nofLayers - 1][idx].mappingCost;
    
    for (int32_t i = nofLayers - 1; i >= 0; --i) {
        auto info = mem[i][idx];
        mapSeq.mappingV.push_back(info.m);
        idx = info.parent;
    }
    
    std::reverse(mapSeq.mappingV.begin(), mapSeq.mappingV.end());
    return mapSeq;
}

SwapSeq BoundedMappingTreeQAllocator::getTransformingSwapsFor(const Mapping& fromM,
                                                              Mapping toM) {

    for (uint32_t i = 0; i < mVQubits; ++i) {
        if (fromM[i] != _undef && toM[i] == _undef) {
            ERR << "Assumption that previous mappings have same mapped qubits "
                << "than current mapping broken." << std::endl;
            ExitWith(ExitCode::EXIT_unreachable);
        } else if (fromM[i] == _undef && toM[i] != _undef) {
            toM[i] = _undef;
        }
    }

    auto fromA = GenAssignment(mPQubits, fromM, false);
    auto toA = GenAssignment(mPQubits, toM, false);

    return mTSFinder->find(fromA, toA);
}

MappingSwapSequence
BoundedMappingTreeQAllocator::phase2(const CandidateVCollection& collection) {
    // Second Phase:
    //     here, the idea is to use, perhaps, dynamic programming to test all possibilities
    //     for 'glueing' the sequence of collection together.
    uint32_t nofLayers = collection.size();
    uint32_t layerMaxSize = 0;

    for (uint32_t i = 0; i < nofLayers; ++i)
        layerMaxSize = std::max(layerMaxSize, (uint32_t) collection[i].size());
    
    INF << "PHASE 2 >>>> Dynamic Programming" << std::endl;
    INF << "Layers: " << nofLayers << std::endl;
    INF << "MaxSize: " << layerMaxSize << std::endl;

    TIMatrix mem(nofLayers, TIVector());

    for (uint32_t i = 0, e = collection[0].size(); i < e; ++i) {
        mem[0].push_back({ collection[0][i].m, _undef, collection[0][i].cost, 0 });
    }

    for (uint32_t i = 1; i < nofLayers; ++i) {
        INF << "Beginning: " << i << " of " << nofLayers << " layers." << std::endl;

        uint32_t jLayerSize = collection[i].size();
        for (uint32_t j = 0; j < jLayerSize; ++j) {
            Timer jt;
            jt.start();

            TracebackInfo best = { {}, _undef, _undef, 0 };
            uint32_t kLayerSize = collection[i - 1].size();

            for (uint32_t k = 0; k < kLayerSize; ++k) {
                auto mapping = collection[i][j].m;
                auto lastMapping = mem[i - 1][k].m;

                mLQPProcessor->process(mArchGraph.get(), lastMapping, mapping);

                // uint32_t mappingCost = mem[i - 1][k].mappingCost;
                // Should be this one.
                uint32_t mappingCost = mem[i - 1][k].mappingCost + collection[i][j].cost;
                uint32_t swapEstimatedCost = mCostEstimator->estimate(lastMapping, mapping) +
                    mem[i - 1][k].swapEstimatedCost;

                if (mappingCost + swapEstimatedCost < best.mappingCost + best.swapEstimatedCost) {
                    best = { mapping, k, mappingCost, swapEstimatedCost };
                }
            }

            mem[i].push_back(best);

            jt.stop();
            INF << "(i:" << i << ", j:" << j << "): "
                << ((double) jt.getMilliseconds()) / 1000.0 << std::endl;
        }

        INF << "End: " << i << " of " << nofLayers << " layers." << std::endl;
    }

    Vector mapSequenceIndexes = mMSSelector->select(mem);
    MappingSwapSequence best = { {}, {}, _undef };

    for (uint32_t idx : mapSequenceIndexes) {
        // mapSCollection.push_back(tracebackPath(mem, idx));
        SwapSeqCollection swapSeqCollection;
        auto seq = tracebackPath(mem, idx);

        uint32_t swapCost = 0;
        uint32_t mappingCost = seq.mappingCost * RevCost.getVal();

        for (uint32_t i = 1; i < nofLayers; ++i) {
            auto swaps = getTransformingSwapsFor(seq.mappingV[i - 1], seq.mappingV[i]);
            swapSeqCollection.push_back(swaps);
            swapCost += swaps.size() * SwapCost.getVal();
        }

        if (swapCost + mappingCost < best.cost) {
            best.mappingV = seq.mappingV;
            best.swapSeqCollection = swapSeqCollection;
            best.cost = swapCost + mappingCost;
        }
    }

    return best;
}


StdSolution BoundedMappingTreeQAllocator::phase3(const MappingSwapSequence& mss) {
    // Third Phase:
    //     build the operations vector by tracebacking from the solution we have
    //     found. For this, we have to go through every dependency again.
    StdSolution sol;

    sol.mCost = 0;
    sol.mInitial.assign(mVQubits, 0);

    uint32_t idx = 0;
    auto mapping = mss.mappingV[idx];

    Mapping realToDummy = mapping;
    Mapping dummyToPhys = IdentityMapping(mPQubits);

    for (auto& partition : mPP) {
        for (auto& node : partition) {
        // for (auto& iDependencies : deps) {
            // We are sure that there are no instruction dependency that has more than
            // one dependency.
            auto iDependencies = mDBuilder.getDeps(node);
            if (iDependencies.getSize() < 1)
                continue;

            auto dep = iDependencies[0];

            uint32_t a = dep.mFrom, b = dep.mTo;
            uint32_t u = mapping[a], v = mapping[b];

            StdSolution::OpVector opVector;

            // If we can't satisfy (u, v) with the current mapping, it can only mean
            // that we must go to the next one.
            if ((u == _undef || v == _undef) ||
                (!mArchGraph->hasEdge(u, v) && !mArchGraph->hasEdge(v, u))) {

                if (++idx >= mss.mappingV.size()) {
                    ERR << "Not enough mappings were generated, maybe!?" << std::endl;
                    ERR << "Mapping for '" << iDependencies.mCallPoint->toString(false)
                        << "'." << std::endl;
                    ExitWith(ExitCode::EXIT_unreachable);
                }

                mapping = mss.mappingV[idx];

                auto swaps = mss.swapSeqCollection[idx - 1];
                auto physToDummy = GenAssignment(mPQubits, dummyToPhys);

                for (auto swp : swaps) {
                    uint32_t a = physToDummy[swp.u], b = physToDummy[swp.v];

                    if (!mArchGraph->hasEdge(swp.u, swp.v)) {
                        std::swap(a, b);
                    }

                    std::swap(physToDummy[swp.u], physToDummy[swp.v]);
                    std::swap(dummyToPhys[a], dummyToPhys[b]);
                    opVector.push_back({ Operation::K_OP_SWAP, a, b });
                }

                sol.mCost += swaps.size() * SwapCost.getVal();

                for (uint32_t i = 0; i < mVQubits; ++i) {
                    if (realToDummy[i] == _undef && mapping[i] != _undef) {
                        realToDummy[i] = physToDummy[mapping[i]];
                    }
                }

                u = mapping[a];
                v = mapping[b];
            }

            Operation op;
            op.mU = a;
            op.mV = b;

            if (mArchGraph->hasEdge(u, v)) {
                op.mK = Operation::K_OP_CNOT;
            } else if (mArchGraph->hasEdge(v, u)) {
                sol.mCost += RevCost.getVal();
                op.mK = Operation::K_OP_REV;
            } else {
                ERR << "Mapping " << MappingToString(mapping) << " not able to satisfy dependency "
                    << "(" << a << "{" << u << "}, " << b << "{" << v << "})" << std::endl;
                ExitWith(ExitCode::EXIT_unreachable);
            }

            opVector.push_back(op);
            sol.mOpSeqs.push_back(std::make_pair(node, opVector));
        }
    }

    Fill(mVQubits, realToDummy);
    sol.mInitial = realToDummy;

    auto dummyToReal = GenAssignment(mVQubits, realToDummy, false);

    // Transforming swaps from dummy to real.
    for (auto& pair : sol.mOpSeqs) {
        for (auto& op : pair.second) {
            if (op.mK == Operation::K_OP_SWAP) {
                op.mU = dummyToReal[op.mU];
                op.mV = dummyToReal[op.mV];
            }
        }
    }

    return sol;
}

StdSolution BoundedMappingTreeQAllocator::buildStdSolution(QModule::Ref qmod) {
    if (mNCIterator.get() == nullptr) {
        mNCIterator.reset(new SeqNCandidateIterator(qmod->stmt_begin(), qmod->stmt_end()));
    }

    if (mChildrenCSelector.get() == nullptr) {
        mChildrenCSelector.reset(new FirstCandidateSelector());
    }

    if (mPartialSolutionCSelector.get() == nullptr) {
        mPartialSolutionCSelector.reset(new FirstCandidateSelector());
    }

    if (mCostEstimator.get() == nullptr) {
        mCostEstimator.reset(new GeoDistanceSwapCEstimator());
    }

    if (mLQPProcessor.get() == nullptr) {
        mLQPProcessor.reset(new GeoNearestLQPProcessor());
    }

    if (mMSSelector.get() == nullptr) {
        mMSSelector.reset(new BestMSSelector());
    }

    if (mTSFinder.get() == nullptr) {
        mTSFinder.reset(new ApproxTSFinder(mArchGraph));
    }

    mCostEstimator->fixGraph(mArchGraph.get());

    mMaxChildren = MaxChildren.getVal();
    mMaxPartial = MaxPartialSolutions.getVal();

    mDBuilder = PassCache::Get<DependencyBuilderWrapperPass>(qmod)->getData();
    uint32_t nofDeps = mDBuilder.getDependencies().size();

    StdSolution sol;

    sol.mCost = 0;
    sol.mInitial = IdentityMapping(mPQubits);

    if (nofDeps > 0) {
        auto phase1Output = phase1();
        auto phase2Output = phase2(phase1Output);
        sol = phase3(phase2Output);
    }

    return sol;
}

BoundedMappingTreeQAllocator::uRef
BoundedMappingTreeQAllocator::Create(ArchGraph::sRef ag) {
    return uRef(new BoundedMappingTreeQAllocator(ag));
}