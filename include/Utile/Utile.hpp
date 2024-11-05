
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
        : elems(elements), currentPermutation(permutationSize, elements[0]), isFinished(elements.empty()) {}

    bool hasNext() const {
        return !isFinished;
    }

    std::vector<T> next() {
        if (!hasNext()) {
            throw std::out_of_range("No more permutations");
        }

        std::vector<T> result = currentPermutation;  // Copy the current permutation

        // Find the next permutation with repetition
        for (std::size_t i = currentPermutation.size() - 1; i >= 0; --i) {
            std::size_t nextIndex = std::find(elems.begin(), elems.end(), currentPermutation[i]) - elems.begin() + 1;
            if (nextIndex < elems.size()) {
                currentPermutation[i] = elems[nextIndex];
                break;
            } else {
                currentPermutation[i] = elems[0];
                if (i == 0) {
                    isFinished = true;  // All elements are rolled over to the start, end of permutations
                }
            }
        }

        return result;
    }

private:
    const std::vector<T>& elems;
    std::vector<T> currentPermutation;
    bool isFinished;
};



}