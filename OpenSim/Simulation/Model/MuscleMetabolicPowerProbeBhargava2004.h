#ifndef OPENSIM_METABOLIC_POWER_PROBE_BHARGAVA2004_H_
#define OPENSIM_METABOLIC_POWER_PROBE_BHARGAVA2004_H_
/* -------------------------------------------------------------------------- *
 *             OpenSim:  MuscleMetabolicPowerProbeBhargava2004.h              *
 * -------------------------------------------------------------------------- *
 * The OpenSim API is a toolkit for musculoskeletal modeling and simulation.  *
 * See http://opensim.stanford.edu and the NOTICE file for more information.  *
 * OpenSim is developed at Stanford University and supported by the US        *
 * National Institutes of Health (U54 GM072970, R24 HD065690) and by DARPA    *
 * through the Warrior Web program.                                           *
 *                                                                            *
 * Copyright (c) 2005-2012 Stanford University and the Authors                *
 * Author(s): Tim Dorn                                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied    *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include "Probe.h"
#include "Model.h"
#include "MetabolicMuscleParameterSet.h"
#include <OpenSim/Common/PiecewiseLinearFunction.h>

namespace OpenSim { 

//=============================================================================
//             MUSCLE METABOLIC POWER PROBE (Bhargava, et al., 2004)
//=============================================================================
/**
 * MuscleMetabolicPowerProbeBhargava2004 is a ModelComponent Probe for computing the 
 * net metabolic energy rate of a set of Muscles in the model during a simulation. 
 * 
 * Based on the following paper:
 *
 * <a href="http://www.ncbi.nlm.nih.gov/pubmed/14672571">
 * Bhargava, L. J., Pandy, M. G. and Anderson, F. C. (2004). 
 * A phenomenological model for estimating metabolic energy consumption
 * in muscle contraction. J Biomech 37, 81-8.</a>
 *
 * <I>Note that the equations below that describe the particular implementation of 
 * MuscleMetabolicPowerProbeBhargava2004 may slightly differ from the equations
 * described in the representative publication above. Note also that we define
 * positive muscle velocity to indicate lengthening (eccentric contraction) and
 * negative muscle velocity to indicate shortening (concentric contraction).</I>
 *
 *
 * Muscle metabolic power (or rate of metabolic energy consumption) is equal to the
 * rate at which heat is liberated plus the rate at which work is done:\n
 * <B>Edot = Bdot + sumOfAllMuscles(Adot + Mdot + Sdot + Wdot).</B>
 *
 *       - Bdot is the basal heat rate.
 *       - Adot is the activation heat rate.
 *       - Mdot is the maintenance heat rate.
 *       - Sdot is the shortening heat rate.
 *       - Wdot is the mechanical work rate.
 *
 *
 * This probe also uses muscle parameters stored in the MetabolicMuscle object for each muscle.
 * The full set of all MetabolicMuscles (MetabolicMuscleSet) is a property of this probe:
 * 
 * - m = The mass of the muscle (kg).
 * - r = Ratio of slow twitch fibers in the muscle (must be between 0 and 1).
 * - Adot_slow = Activation constant for slow twitch fibers (W/kg).
 * - Adot_fast = Activation constant for fast twitch fibers (W/kg).
 * - Mdot_slow = Maintenance constant for slow twitch fibers (W/kg).
 * - Mdot_fast = Maintenance constant for slow twitch fibers (W/kg).
 *
 *
 * <H2><B> BASAL HEAT RATE </B></H2>
 * If <I>basal_rate_on</I> is set to true, then Bdot is calculated as follows:\n
 * <B>Bdot = basal_coefficient * (m_body^basal_exponent)</B>
 *     - m_body = mass of the entire model
 *     - basal_coefficient and basal_exponent are defined by their respective properties.\n
 * <I>Note that this quantity is muscle independant. Rather it is calculated on a whole body level.</I>
 *
 *
 * <H2><B> ACTIVATION HEAT RATE </B></H2>
 * If <I>activation_rate_on</I> is set to true, then Adot is calculated as follows:\n
 * <B>Adot = m * [ Adot_slow * r * sin((pi/2)*u)    +    Adot_fast * (1-r) * (1-cos((pi/2)*u)) ]</B>
 *     - u = muscle excitation at the current time.
 *
 *
 * <H2><B> MAINTENANCE HEAT RATE </B></H2>
 * If <I>maintenance_rate_on</I> is set to true, then Mdot is calculated as follows:\n
 * <B>Mdot = m * f * [ Mdot_slow * r * sin((pi/2)*u)    +    Mdot_fast * (1-r) * (1-cos((pi/2)*u)) ]</B>
 * - u = muscle excitation at the current time.
 * - f is a piecewise linear function that describes the normalized fiber length denendence
 * of the maintenance heat rate (default curve is shown below):
 * \image html fig_NormalizedFiberLengthDependenceOfMaintenanceHeatRateBhargava2004.png
 *
 *
 * <H2><B> SHORTENING HEAT RATE </B></H2>
 * If <I>shortening_rate_on</I> is set to true, then Sdot is calculated as follows:\n
 * <B>Sdot = -alpha * v_CE</B>
 *
 * If use_force_dependent_shortening_prop_constant = true,
 *     - <B>alpha = (0.16 * F_CE_iso) + (0.18 * F_CE)   </B>,   <I>v_CE >= 0 (concentric / isometric contraction)</I>
 *     - <B>alpha = 0.157 * F_CE                        </B>,   <I>v_CE <  0 (eccentric contraction)</I>
 * 
 *     - v_CE = muscle fiber velocity at the current time.
 *     - F_CE = force developed by the contractile element of muscle at the current time.
 *     - F_CE_iso = force that would be developed by the contractile element of muscle under isometric conditions with the current activation and fiber length.
 *
 * If use_force_dependent_shortening_prop_constant = false,
 *     - <B>alpha = 0.25,   </B>,   <I>v_CE >= 0 (concentric / isometric contraction)</I>
 *     - <B>alpha = 0.00    </B>,   <I>v_CE <  0 (eccentric contraction)</I>
 *
 *
 * <H2><B> MECHANICAL WORK RATE </B></H2>
 * If <I>mechanical_work_rate_on</I> is set to true, then Wdot is calculated as follows:\n
 * <B>Wdot = -F_CE * v_CE       </B>,   <I>v_CE >= 0 (concentric / isometric contraction)</I>\n
 * <B>Wdot = 0                  </B>,   <I>v_CE <  0 (eccentric contraction)</I>
 *     - v_CE = muscle fiber velocity at the current time.
 *     - F_CE = force developed by the contractile element of muscle at the current time.\n
 * <I> Note: if normalize_mechanical_work_rate_by_muscle_mass ia set to true, then the mechanical work rate
 *       for each muscle is normalized by its muscle mass (kg).</I>
 *
 *
 * @author Tim Dorn
 */

class OSIMSIMULATION_API MuscleMetabolicPowerProbeBhargava2004 : public Probe {
OpenSim_DECLARE_CONCRETE_OBJECT(MuscleMetabolicPowerProbeBhargava2004, Probe);
public:
//==============================================================================
// PROPERTIES
//==============================================================================
    /** @name Property declarations
    These are the serializable properties associated with this class. **/
    /**@{**/
    /** Enabled by default. **/
    OpenSim_DECLARE_PROPERTY(activation_rate_on, 
        bool,
        "Specify whether the activation heat rate is to be calculated (true/false).");

    /** Enabled by default. **/
    OpenSim_DECLARE_PROPERTY(maintenance_rate_on, 
        bool,
        "Specify whether the maintenance heat rate is to be calculated (true/false).");

    /** Enabled by default. **/
    OpenSim_DECLARE_PROPERTY(shortening_rate_on, 
        bool,
        "Specify whether the shortening heat rate is to be calculated (true/false).");

    /** Enabled by default. **/
    OpenSim_DECLARE_PROPERTY(basal_rate_on, 
        bool,
        "Specify whether the basal heat rate is to be calculated (true/false).");

    /** Enabled by default. **/
    OpenSim_DECLARE_PROPERTY(mechanical_work_rate_on, 
        bool,
        "Specify whether the mechanical work rate is to be calculated (true/false).");

    /** Default curve shown in doxygen. **/
    OpenSim_DECLARE_PROPERTY(normalized_fiber_length_dependence_on_maintenance_rate, 
        PiecewiseLinearFunction,
        "Contains a PiecewiseLinearFunction object that describes the normalized fiber length dependence on maintenance rate.");

    /** Disabled by default. **/
    OpenSim_DECLARE_PROPERTY(use_force_dependent_shortening_prop_constant, 
        bool,
        "Specify whether to use a force dependent shortening proportionality constant (true/false).");

    /** Default value = 1.51. **/
    OpenSim_DECLARE_PROPERTY(basal_coefficient, 
        double,
        "Basal metabolic coefficient.");

    /** Default value = 1.0. **/
    OpenSim_DECLARE_PROPERTY(basal_exponent, 
        double,
        "Basal metabolic exponent.");

    /** Disabled by default. **/
    OpenSim_DECLARE_PROPERTY(normalize_mechanical_work_rate_by_muscle_mass, 
        bool,
        "Specify whether the mechanical work rate for each muscle is to be normalized by muscle mass (true/false).");

    OpenSim_DECLARE_UNNAMED_PROPERTY(MetabolicMuscleParameterSet,
        "A MetabolicMuscleSet containing the muscle information required to calculate metabolic energy expenditure. "
        "If multiple muscles are contained in the set, then the probe value will equal the summation of all metabolic powers.");

    /**@}**/

//=============================================================================
// PUBLIC METHODS
//=============================================================================
    //--------------------------------------------------------------------------
    // Constructor(s) and Setup
    //--------------------------------------------------------------------------
    /** Default constructor */
    MuscleMetabolicPowerProbeBhargava2004();
    /** Convenience constructor */
    MuscleMetabolicPowerProbeBhargava2004(bool activation_rate_on, bool maintenance_rate_on, 
        bool shortening_rate_on, bool basal_rate_on, bool work_rate_on);


    //-----------------------------------------------------------------------------
    // Computation
    //-----------------------------------------------------------------------------
    /** Compute muscle metabolic power. */
    virtual SimTK::Vector computeProbeInputs(const SimTK::State& state) const OVERRIDE_11;

    /** Returns the number of probe inputs in the vector returned by computeProbeInputs(). */
    int getNumProbeInputs() const OVERRIDE_11;

    /** Returns the column labels of the probe values for reporting. 
        Currently uses the Probe name as the column label, so be sure
        to name your probe appropiately!*/
    virtual OpenSim::Array<std::string> getProbeOutputLabels() const OVERRIDE_11;

    /** Check that the MetabolicMuscleParameter represents a valid muscle in the model. */
    Muscle* checkValidMetabolicMuscle(MetabolicMuscleParameter mm) const;


//==============================================================================
// PRIVATE
//==============================================================================
private:
    //--------------------------------------------------------------------------
    // ModelComponent Interface
    //--------------------------------------------------------------------------
    void connectToModel(Model& aModel) OVERRIDE_11;

    void setNull();
    void constructProperties();


//=============================================================================
};	// END of class MuscleMetabolicPowerProbeBhargava2004

}; //namespace
//=============================================================================
//=============================================================================


#endif // #ifndef OPENSIM_METABOLIC_POWER_PROBE_BHARGAVA2004_H_