function exampleSitToStand

%% Part 0: Load the OpenSim and Moco libraries.
import org.opensim.modeling.*;

%% Part 1: Torque-driven Predictive Problem
% Part 1a: Create a new MocoStudy.

% Part 1b: Initialize the problem and set the model.

% Part 1c: Set bounds on the problem.
% Time bounds

% Position bounds: the model should start in a crouch and finish standing up.

% Velocity bounds: the model coordinates should start and end at rest.
% This function accepts string patterns to set multiple state infos
% at once. The '.*' is replaced to match any states compatible with the pattern.
% The empty brackets indicate that the default speed range, [-50, 50], is
% used for all speed states.

% Part 1d: Add a MocoControlCost to the problem.

% Part 1e: Configure the solver.

% Part 1f: Solve! Write the solution to file, and visualize.

%% Part 2: Torque-driven Tracking Problem
% Part 2a: Construct a tracking reference TimeSeriesTable using filtered data
% from the previous solution. Use a TableProcessor, which accepts a base table
% and can append operations to modify the table.

% Part 2b: Add a MocoStateTrackingCost to the problem using the states
% from the predictive problem (via the TableProcessor we just created), and set
% weights to zero for states associated with the dependent coordinate in the
% model's knee CoordinateCoupler constraint. 

% Part 2c: Reduce the control cost weight so it now acts as a regularization 
% term.

% Part 2d: Set the initial guess using the predictive problem solution.

% Part 2e: Solve! Write the solution to file, and visualize.

%% Part 3: Compare Predictive and Tracking Solutions
% This is a convenience function provided for you. See mocoPlotTrajectory.m
mocoPlotTrajectory(predictSolution, trackingSolution, 'predict', 'track');

%% Part 4: Muscle-driven Inverse Problem
% Create a MocoInverse tool instance.
inverse = MocoInverse();

% Part 4a: Provide the model via a ModelProcessor. Similar to the TableProcessor,
% you can add operators to modify the base model.

% Part 4b: Set the reference kinematics using the same TableProcessor we used
% in the tracking problem.

% Set the time range and allow extra (unused) columns in the kinematics.
inverse.set_kinematics_allow_extra_columns(true);
inverse.set_initial_time(0);
inverse.set_final_time(1);

% Set the mesh interval and convergence tolerance, and enable minimizing
% muscle activation states.
inverse.set_mesh_interval(0.05);
inverse.set_tolerance(1e-4);
inverse.set_minimize_sum_squared_states(true);

% Part 4c: Append additional outputs path for quantities that are calculated
% post-hoc using the inverse problem solution.
inverse.append_output_paths('.*normalized_fiber_length');
inverse.append_output_paths('.*passive_force_multiplier');

% Part 4d: Solve! Write the solution and outputs.

%% Part 5: Muscle-driven Inverse Problem with Passive Assistance
% Part 5a: Create a new muscle-driven model, now adding a SpringGeneralizedForce 
% about the knee coordinate.

% Create a ModelProcessor similar to the previous one, using the same
% reserve actuator strength so we can compare muscle activity accurately.

% Part 5b: Solve! Write solution.

%% Part 6: Compare unassisted and assisted Inverse Problems.
fprintf('Cost without device: %f\n', ...
        inverseSolution.getMocoSolution().getObjective());
fprintf('Cost with device: %f\n', ...
        inverseDeviceSolution.getMocoSolution().getObjective());
% This is a convenience function provided for you. See below for the
% implementation.
compareInverseSolutions(inverseSolution, inverseDeviceSolution);

end

%% Model Creation and Plotting Convenience Functions 

function addCoordinateActuator(model, coordName, optForce)

import org.opensim.modeling.*;

coordSet = model.updCoordinateSet();

actu = CoordinateActuator();
actu.setName(['tau_' coordName]);
actu.setCoordinate(coordSet.get(coordName));
actu.setOptimalForce(optForce);
actu.setMinControl(-1);
actu.setMaxControl(1);

model.addComponent(actu);

end

function [model] = getTorqueDrivenModel()

import org.opensim.modeling.*;

% Load the base model.
model = Model('sitToStand_3dof9musc.osim');

% Remove the muscles in the model.
model.updForceSet().clearAndDestroy();
model.initSystem();

% Add CoordinateActuators to the model degrees-of-freedom.
addCoordinateActuator(model, 'hip_flexion_r', 150);
addCoordinateActuator(model, 'knee_angle_r', 300);
addCoordinateActuator(model, 'ankle_angle_r', 150);

end

function compareInverseSolutions(unassistedSolution, assistedSolution)

unassistedSolution = unassistedSolution.getMocoSolution();
assistedSolution = assistedSolution.getMocoSolution();
figure;
stateNames = unassistedSolution.getStateNames();
numStates = stateNames.size();
dim = 3;
iplot = 0;
for i = 0:numStates-1
    if contains(char(stateNames.get(i)), 'activation')
        iplot = iplot + 1;
        subplot(dim, dim, iplot);
        plot(unassistedSolution.getTimeMat(), ...
             unassistedSolution.getStateMat(stateNames.get(i)), '-r', ...
             'linewidth', 3);
        hold on
        plot(assistedSolution.getTimeMat(), ...
             assistedSolution.getStateMat(stateNames.get(i)), '--b', ...
             'linewidth', 2.5);
        hold off
        stateName = stateNames.get(i).toCharArray';
        plotTitle = stateName;
        plotTitle = strrep(plotTitle, '/forceset/', '');
        plotTitle = strrep(plotTitle, '/activation', '');
        title(plotTitle, 'Interpreter', 'none');
        xlabel('time (s)');
        ylabel('activation (-)');
        ylim([0, 1]);
        if iplot == 0
           legend('unassisted', 'assisted');
        end
    end
end

end
