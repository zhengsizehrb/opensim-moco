#ifndef MOCO_CASOCTRANSCRIPTION_H
#define MOCO_CASOCTRANSCRIPTION_H
/* -------------------------------------------------------------------------- *
 * OpenSim Moco: CasOCTranscription.h                                         *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2018 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Christopher Dembia                                              *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0          *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include "CasOCSolver.h"

namespace CasOC {

/// This is the base class for transcription schemes that convert a
/// CasOC::Problem into a general nonlinear programming problem. If you are
/// creating a new derived class, make sure to override all virtual functions
/// and obey the settings that the user specified in the CasOC::Solver. Build
/// the CasADi problem in the constructor of your derived class by defining the
/// following member variables in this class:
/// - m_vars
/// - m_lowerBounds
/// - m_upperBounds
/// Use setObjective() and addConstraints() to specify the functions in the
/// optimization problem.
class Transcription {
public:
    Transcription(const Solver& solver, const Problem& problem, 
        const int& numGridPoints) : m_solver(solver), m_problem(problem),
        m_numGridPoints(numGridPoints) {}
    virtual ~Transcription() = default;
    Iterate createInitialGuessFromBounds() const;
    Iterate createRandomIterateWithinBounds() const;
    template <typename T>
    T createTimes(const T& initialTime, const T& finalTime) const {
        return (finalTime - initialTime) * m_grid + initialTime;
    }
    casadi::DM createQuadratureCoefficients() const  {
        return createQuadratureCoefficientsImpl();
    }
    casadi::DM createKinematicConstraintIndices() const  {
        return createKinematicConstraintIndicesImpl();
    }
    void applyConstraints() {
        applyConstraintsImpl();
    }
    Solution solve(const Iterate& guessOrig) {
        auto guess = guessOrig.resample(
                createTimes(guessOrig.variables.at(Var::initial_time),
                        guessOrig.variables.at(Var::final_time)));

        // Option handling is copied from casadi::OptiNode::solver().
        casadi::Dict options = m_solver.getPluginOptions();
        if (!options.empty()) {
            options[m_solver.getOptimSolver()] = m_solver.getSolverOptions();
        }
        casadi::MXDict nlp;
        nlp.emplace(std::make_pair("x", flatten(m_vars)));
        nlp.emplace(std::make_pair("f", m_objective));
        nlp.emplace(std::make_pair("g", casadi::MX::veccat(m_constraints)));
        auto nlpFunc =
                casadi::nlpsol("nlp", m_solver.getOptimSolver(), nlp, options);

        // Run the optimization.
        // ---------------------
        const casadi::DMDict nlpResult =
                nlpFunc(casadi::DMDict{{"x0", flatten(guess.variables)},
                        {"lbx", flatten(m_lowerBounds)},
                        {"ubx", flatten(m_upperBounds)},
                        {"lbg", casadi::DM::veccat(m_constraintsLowerBounds)},
                        {"ubg", casadi::DM::veccat(m_constraintsUpperBounds)}});
        Solution solution = m_problem.createIterate<Solution>();
        solution.variables = expand(nlpResult.at("x"));

        solution.times = createTimes(solution.variables[Var::initial_time],
                solution.variables[Var::final_time]);
        solution.stats = nlpFunc.stats();
        return solution;
    }

protected:
    /// This must be called in the constructor for derived classes of so that 
    /// overridden virtual methods are accessible to the base class. This
    /// implementation allows initialization to occur during construction, 
    /// avoiding an extra call on the instantiated object. 
    void transcribe();

    template <typename TRow, typename TColumn>
    void setVariableBounds(Var var, const TRow& rowIndices,
            const TColumn& columnIndices, const Bounds& bounds) {
        if (bounds.isSet()) {
            const auto& lower = bounds.lower;
            m_lowerBounds[var](rowIndices, columnIndices) = lower;
            const auto& upper = bounds.upper;
            m_upperBounds[var](rowIndices, columnIndices) = upper;
        } else {
            m_lowerBounds[var](rowIndices, columnIndices) =
                    -std::numeric_limits<double>::infinity();
            m_upperBounds[var](rowIndices, columnIndices) =
                    std::numeric_limits<double>::infinity();
        }
    }

    void calcDAE(casadi_int itime, const int& NQ, casadi::MX& xdot, 
        bool calcQErr, casadi::MX& qerr);

    void setObjective(casadi::MX objective) {
        m_objective = std::move(objective);
    }

    void addConstraints(const casadi::DM& lower, const casadi::DM& upper,
            const casadi::MX& equations);

    const Solver& m_solver;
    const Problem& m_problem;
    VariablesMX m_vars;
    VariablesDM m_lowerBounds;
    VariablesDM m_upperBounds;
    // The grid for a transcription scheme includes both mesh points (i.e. 
    // points that lie on the endpoints of a mesh interval) and any additional
    // collocation points that may lie on mesh interior (as in Hermite-Simpson
    // collocation, etc.).
    int m_numGridPoints;
    casadi::DM m_grid;
    casadi::MX m_times;
    casadi::MX m_duration;
    casadi::MX xdot;
    casadi::MX qerr;
    

private:
    casadi::MX m_objective;
    std::vector<casadi::MX> m_constraints;
    std::vector<casadi::DM> m_constraintsLowerBounds;
    std::vector<casadi::DM> m_constraintsUpperBounds;

private:
    /// Override this function in your derived class to compute a vector of
    /// quadrature coeffecients (of length m_numGridPoints) required to set the
    /// the integral cost within transcribe().
    virtual casadi::DM createQuadratureCoefficientsImpl() const = 0;
    /// Override this function to specify the indicies in the grid where any
    /// existing kinematic constraints are to be enforced.
    /// @note The returned vector must be of length m_numGridPoints with nonzero
    /// values at the indices where kinematic constraints are enforced.
    virtual casadi::DM createKinematicConstraintIndicesImpl() const = 0;
    /// Override this function in your derived class set the defect, kinematic,
    /// and path constraint errors required for your transcription scheme.
    virtual void applyConstraintsImpl() = 0;

    /// Use this function to ensure you iterate through variables in the same
    /// order.
    template <typename T>
    static std::vector<Var> getSortedVarKeys(const Variables<T>& vars) {
        std::vector<Var> keys;
        for (const auto& kv : vars) { keys.push_back(kv.first); }
        std::sort(keys.begin(), keys.end());
        return keys;
    }
    /// Convert the map of variables into a column vector, for passing onto
    /// nlpsol(), etc.
    template <typename T>
    static T flatten(const CasOC::Variables<T>& vars) {
        std::vector<T> stdvec;
        for (const auto& key : getSortedVarKeys(vars)) {
            stdvec.push_back(vars.at(key));
        }
        return T::veccat(stdvec);
    }
    /// Convert the 'x' column vector into separate variables.
    CasOC::VariablesDM expand(const casadi::DM& x) const {
        CasOC::VariablesDM out;
        using casadi::Slice;
        casadi_int offset = 0;
        for (const auto& key : getSortedVarKeys(m_vars)) {
            const auto& value = m_vars.at(key);
            // Convert a portion of the column vector into a matrix.
            out[key] = casadi::DM::reshape(
                    x(Slice(offset, offset + value.numel())), value.rows(),
                    value.columns());
            offset += value.numel();
        }
        return out;
    }
};

} // namespace CasOC

#endif // MOCO_CASOCTRANSCRIPTION_H