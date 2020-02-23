//  April/2019, ETHZ, Jaeyoung Lim, jalim@student.ethz.ch

#include "reciprocal_avoidance/single_vehicle.h"


//Constructor
SingleVehicle::SingleVehicle(const ros::NodeHandle& nh,
                             const ros::NodeHandle& nh_private)
  : SingleVehicle(nh, nh_private, "uav1") {}


SingleVehicle::SingleVehicle(const ros::NodeHandle& nh,
                             const ros::NodeHandle& nh_private,
                             std::string name):
  nh_(nh),
  nh_private_(nh_private),
  vehicle_name_(name) {

  cmdloop_timer_ = nh_.createTimer(ros::Duration(0.03), &SingleVehicle::cmdloopCallback, this); // Define timer for constant loop rate
  statusloop_timer_ = nh_.createTimer(ros::Duration(1), &SingleVehicle::statusloopCallback, this); // Define timer for constant loop rate

  nh_private_.param<std::string>("vehicle_name", vehicle_name_, "uav");

  localsetpoint_publisher_ = nh_.advertise<mavros_msgs::PositionTarget>("/"+vehicle_name_+"/mavros/setpoint_raw/local", 1);
  globalsetpoint_publisher_ = nh_.advertise<mavros_msgs::GlobalPositionTarget>("/"+vehicle_name_+"/mavros/setpoint_raw/global", 1);

  mavstateSub_ = nh_.subscribe("/"+vehicle_name_+"/mavros/state", 1, &SingleVehicle::mavstateCallback, this,ros::TransportHints().tcpNoDelay());

  /**
  * @todo Subscribe to mavstate for mode swtiching 
  * @body Vehicle doesnt arm as the offboard mode is not observed
  */
  arming_client_ = nh_.serviceClient<mavros_msgs::CommandBool>("/"+vehicle_name_+"/mavros/cmd/arming");
  set_mode_client_ = nh_.serviceClient<mavros_msgs::SetMode>("/"+vehicle_name_+"/mavros/set_mode");

  sim_enable_ = true;
  setpoint_type_ = SETPOINT_TYPE::GLOBAL_SETPOINT;
}

SingleVehicle::~SingleVehicle() {
  //Destructor
}

void SingleVehicle::cmdloopCallback(const ros::TimerEvent& event){
  switch(setpoint_type_){
    case SETPOINT_TYPE::LOCAL_SETPOINT :
      PublishLocalSetpoint();
      break;
    
    case SETPOINT_TYPE::GLOBAL_SETPOINT :
      PublishGlobalSetpoint();
      break;

  }
}

void SingleVehicle::statusloopCallback(const ros::TimerEvent& event){
    if(sim_enable_){
    // Enable OFFBoard mode and arm automatically
    // This is only run if the vehicle is simulated
    /**
    * @todo Dangerous behaviros in  automatic disarming / rearming
    * @body This will rearm the vehicle in case of a failure
    */
    arm_cmd_.request.value = true;
    offb_set_mode_.request.custom_mode = "OFFBOARD";
    if( current_state_.mode != "OFFBOARD" && (ros::Time::now() - last_request_ > ros::Duration(5.0))){
      if( set_mode_client_.call(offb_set_mode_) && offb_set_mode_.response.mode_sent){
        ROS_INFO("Setting to Offboard");
      }
      last_request_ = ros::Time::now();
    } else {
      if( !current_state_.armed && (ros::Time::now() - last_request_ > ros::Duration(5.0))){
        if( arming_client_.call(arm_cmd_) && arm_cmd_.response.success){
          ROS_INFO("Vehicle armed");
        }
        last_request_ = ros::Time::now();
      }
    }
  }
}

void SingleVehicle::mavstateCallback(const mavros_msgs::State::ConstPtr& msg){
    current_state_ = *msg;
}

void SingleVehicle::SetReferenceState(Eigen::Vector3d ref_position, Eigen::Vector3d ref_velocity){
  reference_pos_ = ref_position;
  reference_vel_ = ref_velocity;

}

void SingleVehicle::PublishLocalSetpoint(){

  mavros_msgs::PositionTarget msg;
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = "map";
  msg.type_mask = 64;
  msg.position.x = reference_pos_(0);
  msg.position.y = reference_pos_(1);
  msg.position.z = reference_pos_(2);
  msg.velocity.x = reference_vel_(0);
  msg.velocity.y = reference_vel_(1);
  msg.velocity.z = reference_vel_(2);
  msg.yaw =0.0;
  msg.yaw_rate = 0.0;

  localsetpoint_publisher_.publish(msg);
}

void SingleVehicle::PublishGlobalSetpoint(){

  mavros_msgs::GlobalPositionTarget msg;
  msg.header.stamp = ros::Time::now();
  msg.header.frame_id = "map";
  msg.type_mask = 64;
  msg.latitude = reference_pos_(0);
  msg.longitude = reference_pos_(1);
  msg.altitude = reference_pos_(2);
  msg.velocity.x = reference_vel_(0);
  msg.velocity.y = reference_vel_(1);
  msg.velocity.z = reference_vel_(2);
  msg.yaw =0.0;
  msg.yaw_rate = 0.0;

  globalsetpoint_publisher_.publish(msg);
}