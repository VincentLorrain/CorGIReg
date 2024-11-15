
#include <vector>
#include <iostream>
#include <algorithm>

#pragma once

namespace CorGIReg {
    
template<typename T>
class CombinationIterator {
public:
    CombinationIterator(const std::vector<T>& elements, size_t combinationSize)
        : mElems(elements), mIndices(combinationSize), mIsFinished(combinationSize > elements.size()) {
        std::iota(mIndices.begin(), mIndices.end(), 0);  // Initialize indices to {0, 1, ..., combinationSize-1}
    }

    bool hasNext() const {
        return !mIsFinished;
    }

    std::vector<T> next() {
        if (!hasNext()) {
            throw std::out_of_range("No more combinations");
        }

        // Retrieve the current combination
        std::vector<T> currentCombination;
        for (int index : mIndices) {
            currentCombination.push_back(mElems[index]);
        }

        // Prepare for the next combination
        mIsFinished = !std::next_permutation(mIndices.begin(), mIndices.end(), [this](int a, int b) {
            return a < b && b < mElems.size();
        });

        return currentCombination;
    }

private:
    const std::vector<T>& mElems;
    std::vector<int> mIndices;
    bool mIsFinished;
};

template<typename T>
class PermutationIterator {
public:
    PermutationIterator(const std::vector<T>& elements, size_t permutationSize)
        : elems(elements), indices(permutationSize, 0), isFinished(elements.empty()) {}

    bool hasNext() const {
        return !isFinished;
    }

    std::vector<T> next() {
        if (!hasNext()) {
            throw std::out_of_range("No more permutations");
        }

        // Build the current permutation based on indices
        std::vector<T> result;
        for (size_t idx : indices) {
            result.push_back(elems[idx]);
        }

        // Generate the next indices
        for (int i = indices.size() - 1; i >= 0; --i) {
            if (indices[i] + 1 < elems.size()) {
                ++indices[i];
                break;
            } else {
                indices[i] = 0;
                if (i == 0) {
                    isFinished = true;
                }
            }
        }

        return result;
    }

private:
    const std::vector<T>& elems;
    std::vector<size_t> indices;
    bool isFinished;
};
}