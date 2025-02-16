#pragma once

#include <map>
#include <memory>
#include <string>

#include "drake/geometry/geometry_roles.h"
#include "drake/geometry/meshcat.h"
#include "drake/geometry/meshcat_visualizer_params.h"
#include "drake/geometry/rgba.h"
#include "drake/geometry/scene_graph.h"
#include "drake/systems/framework/diagram_builder.h"
#include "drake/systems/framework/leaf_system.h"

namespace drake {
namespace geometry {

/** A system wrapper for Meshcat that publishes the current state of a
SceneGraph instance (whose QueryObject-valued output port is connected to this
system's input port).  While this system will add geometry to Meshcat, the
Meshcat instance is also available for users to add their own visualization
alongside the MeshcatVisualizer visualizations.  This can be enormously valuable
for impromptu visualizations.

 @system
 name: MeshcatVisualizer
 input_ports:
 - query_object
 @endsystem

The system uses the versioning mechanism provided by SceneGraph to detect
changes to the geometry so that a change in SceneGraph's data will propagate
to Meshcat.

By default, %MeshcatVisualizer visualizes geometries with the illustration role
(see @ref geometry_roles for more details). It can be configured to visualize
geometries with other roles. Only one role can be specified.  See
DrakeVisualizer which uses the same mechanisms for more details.

Instances of %MeshcatVisualizer created by scalar-conversion will publish to the
same Meshcat instance.
@tparam_nonsymbolic_scalar
*/
template <typename T>
class MeshcatVisualizer final : public systems::LeafSystem<T> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(MeshcatVisualizer)

  /** Creates an instance of %MeshcatVisualizer.

   @param meshcat A Meshcat instance.  This class will assume shared ownership
                  for the lifetime of the object.
   @param params  The set of parameters to control this system's behavior.
   @throws std::exception if `params.publish_period <= 0`.
   @throws std::exception if `params.role == Role::kUnassigned`. */
  explicit MeshcatVisualizer(std::shared_ptr<Meshcat> meshcat,
                             MeshcatVisualizerParams params = {});

  /** Scalar-converting copy constructor. See @ref system_scalar_conversion.
   It should only be used to convert _from_ double _to_ other scalar types.
   */
  template <typename U>
  explicit MeshcatVisualizer(const MeshcatVisualizer<U>& other);

  /** Calls Meschat::Delete(std::string path), with the path set to
   MeshcatVisualizerParams::prefix.  Since this visualizer will only ever add
   geometry under this prefix, this will remove all geometry/transforms added
   by the visualizer, or by a previous instance of this visualizer using the
   same prefix.  Use MeshcatVisualizer::delete_on_intialization_event
   to determine whether this should be called on initialization. */
  void Delete() const;

  /** Returns the QueryObject-valued input port. It should be connected to
   SceneGraph's QueryObject-valued output port. Failure to do so will cause a
   runtime error when attempting to broadcast messages. */
  const systems::InputPort<T>& query_object_input_port() const {
    return this->get_input_port(query_object_input_port_);
  }

  /** Adds a MescatVisualizer and connects it to the given SceneGraph's
   QueryObject-valued output port. See
   MeshcatVisualizer::MeshcatVisualizer(MeshcatVisualizer*,
   MeshcatVisualizerParams) for details. */
  static const MeshcatVisualizer<T>& AddToBuilder(
      systems::DiagramBuilder<T>* builder, const SceneGraph<T>& scene_graph,
      std::shared_ptr<Meshcat> meshcat,
      MeshcatVisualizerParams params = {});

  /** Adds a MescatVisualizer and connects it to the given QueryObject-valued
   output port. See MeshcatVisualizer::MeshcatVisualizer(MeshcatVisualizer*,
   MeshcatVisualizerParams) for details. */
  static const MeshcatVisualizer<T>& AddToBuilder(
      systems::DiagramBuilder<T>* builder,
      const systems::OutputPort<T>& query_object_port,
      std::shared_ptr<Meshcat> meshcat,
      MeshcatVisualizerParams params = {});
  //@}

 private:
  /* MeshcatVisualizer of different scalar types can all access each other's
   data. */
  template <typename>
  friend class MeshcatVisualizer;

  /* The periodic event handler. It tests to see if the last scene description
   is valid (if not, sends the objects) and then sends the transforms.  */
  systems::EventStatus UpdateMeshcat(const systems::Context<T>& context) const;

  /* Makes calls to Meshcat::SetObject to register geometry in SceneGraph. */
  void SetObjects(const SceneGraphInspector<T>& inspector) const;

  /* Makes calls to Meshcat::SetTransform to update the poses from SceneGraph.
   */
  void SetTransforms(const QueryObject<T>& query_object) const;

  /* Handles the initialization event. */
  systems::EventStatus OnInitialization(const systems::Context<T>&) const;

  /* The index of this System's QueryObject-valued input port. */
  int query_object_input_port_{};

  /* Meshcat is mutable because we must send messages (a non-const operation)
   from a const System (e.g. during simulation).  We use shared_ptr instead of
   unique_ptr to facilitate sharing ownership through scalar conversion;
   creating a new Meshcat object during the conversion is not a viable option.
   */
  mutable std::shared_ptr<Meshcat> meshcat_{};

  /* The version of the geometry that was last set in Meshcat by this
   instance. Because the underlying Meshcat is shared, this visualizer has no
   guarantees that the Meshcat state correlates with this value. If the version
   found on the input port differs from this value, SetObjects is called again
   before SetTransforms. This is intended to track the information in meshcat_,
   and is therefore also a mutable member variable (instead of declared state).
   */
  mutable GeometryVersion version_;

  /* A store of the dynamic frames and their path. It is coupled with the
   version_.  This is only for efficiency; it does not represent undeclared
   state. */
  mutable std::map<FrameId, std::string> dynamic_frames_{};

  /* A store of the geometries sent to Meshcat, so that they can be removed if a
   new geometry version appears that does not contain them. */
  mutable std::map<GeometryId, std::string> geometries_{};

  /* The parameters for the visualizer.  */
  MeshcatVisualizerParams params_;
};

/** A convenient alias for the MeshcatVisualizer class when using the `double`
scalar type. */
using MeshcatVisualizerd = MeshcatVisualizer<double>;

}  // namespace geometry

// Define the conversion trait to *only* allow double -> AutoDiffXd conversion.
// Symbolic is not supported yet, and AutoDiff -> double doesn't "make sense".
namespace systems {
namespace scalar_conversion {
template <>
struct Traits<geometry::MeshcatVisualizer> : public NonSymbolicTraits {};
}  // namespace scalar_conversion
}  // namespace systems

}  // namespace drake

DRAKE_DECLARE_CLASS_TEMPLATE_INSTANTIATIONS_ON_DEFAULT_NONSYMBOLIC_SCALARS(
    class ::drake::geometry::MeshcatVisualizer)
