/* (c) Copyright CSIRO 2013. Author: Thomas Lowe
   This software is provided under the terms of Schedule 1 of the license agreement between CSIRO, 3DLM and GeoSLAM.
*/
#include "../include/simple_hexapod_controller/standardIncludes.h"
#include "../include/simple_hexapod_controller/imuCompensation.h"
#include <boost/circular_buffer.hpp> 
#include "../include/simple_hexapod_controller/debugOutput.h"

sensor_msgs::Imu imu;
// target rather than measured data
static Vector3d offsetPos(0.0,0.0,0.0);
static Vector3d offsetVel(0,0,0);
static DebugOutput *debugDraw = NULL;

void calculatePassiveAngularFrequency();
void setCompensationDebug(DebugOutput &debug)
{
  debugDraw = &debug;
}

void imuCallback(const sensor_msgs::Imu &imudata)
{  
  imu = imudata;
}

Pose compensation(const Vector3d &targetAccel, double targetAngularVel)
{
#define PHASE_ANALYSIS
#if defined(PHASE_ANALYSIS)
  calculatePassiveAngularFrequency();
  return Pose::identity();
#endif
  Pose adjust;
  Quat orient;            //Orientation from IMU in quat
  Vector3d accel;         //Accelerations with respect to the IMU
  Vector3d rotation;
  Vector3d angularAcc;
  static Vector3d angularVel(0,0,0);
  adjust.rotation=Quat(Vector3d(0,0,0));
  //static boost::circular_buffer<float> cbx(4,0);
  //static boost::circular_buffer<float> cby(4,1);
  //static boost::circular_buffer<float> cbz(4,2);
  static Vector3d IMUPos(0,0,0);
  static Vector3d IMUVel(0,0,0);
  
  
  
  orient.w = imu.orientation.w;
  orient.x = imu.orientation.x;
  orient.y = imu.orientation.y;
  orient.z = imu.orientation.z;
  accel(1) = -imu.linear_acceleration.x;
  accel(0) = -imu.linear_acceleration.y;
  accel(2) = -imu.linear_acceleration.z;
  
  /*//Compensation for gravity
  Vector3d accelcomp;
  RowVector3d gravityrot; 
  gravityrot=orient.toRotationMatrix()*Vector3d(0,0,-9.81);
  accelcomp(0)=accel(0)-gravityrot(0);
  accelcomp(1)=accel(1)-gravityrot(1);  
  accelcomp(2)=accel(2)-gravityrot(2);  
  ROS_ERROR("ACCEL= %f %f %f", accel(0), accel(1),accel(2));0
  ROS_ERROR("GRAVITYROT= %f %f %f", gravityrot(0), gravityrot(1),gravityrot(2)); 
  ROS_ERROR("ACCELCOMP= %f %f %f", accelcomp(0), accelcomp(1),accelcomp(2));*/
  
//Postion compensation

//#define ZERO_ORDER_FEEDBACK
//#define FIRST_ORDER_FEEDBACK
//#define SECOND_ORDER_FEEDBACK
#define IMUINTEGRATION_FIRST_ORDER
//define IMUINTEGRATION_SECOND_ORDER

#if defined(SECOND_ORDER_FEEDBACK)
  double imuStrength = 1;
  double stiffness = 13; // how strongly/quickly we return to the neutral pose
  Vector3d offsetAcc = imuStrength*(targetAccel-accel+Vector3d(0,0,9.8)) - sqr(stiffness)*offsetPos - 2.0*stiffness*offsetVel;
  offsetVel += offsetAcc*timeDelta;
  offsetPos += offsetVel*timeDelta;
  
#elif defined(FIRST_ORDER_FEEDBACK)
  double imuStrength = 0.5;
  double stiffness = 10; // how strongly/quickly we return to the neutral pose
  offsetVel = imuStrength*(targetAccel-accel+Vector3d(0,0,9.8)) - sqr(stiffness)*offsetPos;
  offsetPos += offsetVel*timeDelta;
  
#elif defined(ZERO_ORDER_FEEDBACK)
  double imuStrength = 0.001;
  cbx.push_back(-imu.linear_acceleration.y); 
  cby.push_back(-imu.linear_acceleration.x); 
  cbz.push_back(-imu.linear_acceleration.z);
  offsetPos(0) = imuStrength * (targetAccel(0)-(cbx[0]+cbx[1]+cbx[2]+cbx[3]+cbx[3])/4);
  offsetPos(1) = imuStrength * (targetAccel(1)-(cby[0]+cby[1]+cby[2]+cby[3]+cbx[3])/4);
  offsetPos(2) = imuStrength * (targetAccel(2)-(cbz[0]+cbz[1]+cbz[2]+cbz[3]+cbx[3])/4 + 9.8);
  //offsetPos = imuStrength * (targetAccel-accel+Vector3d(0,0,9.8));
  
 #elif defined(IMUINTEGRATION_SECOND_ORDER)
  double imuStrength = 2;
  double decayRate = 10;
  IMUPos += IMUVel*timeDelta - decayRate*timeDelta*IMUPos;
  IMUVel += (targetAccel-accel+Vector3d(0, 0, 9.8))*timeDelta - decayRate*timeDelta*IMUVel;  
  Vector3d offsetAcc = -imuStrength*IMUPos;
  
#elif defined(IMUINTEGRATION_FIRST_ORDER)
  double imuStrength = 0.02;
  double decayRate = 1;
 // IMUVel += (targetAccel-accel+Vector3d(0, 0, 9.8))*timeDelta - decayRate*timeDelta*IMUVel; 
  IMUVel = (IMUVel + (targetAccel-accel+Vector3d(0, 0, 9.8))*timeDelta)/(1.0 + decayRate*timeDelta);  
  Vector3d offsetAcc = -imuStrength*IMUVel;
#endif
  
  //Angular body velocity compensation.
  double stiffnessangular=5;
  Vector3d angleDelta = adjust.rotation.toRotationVector();  
  //angularAcc(0)=-sqr(stiffnessangular)*angleDelta(0) + 2.0*stiffnessangular*(imu.angular_velocity.y - angularVel(0));
  //angularAcc(1)=-sqr(stiffnessangular)*angleDelta(1) + 2.0*stiffnessangular*(imu.angular_velocity.x - angularVel(1));
  angularAcc= -sqr(stiffnessangular)*angleDelta + 2.0*stiffnessangular*(Vector3d(0,0,targetAngularVel) - Vector3d(-imu.angular_velocity.y, -imu.angular_velocity.x, -imu.angular_velocity.z) - angularVel);
  angularVel += angularAcc*timeDelta;
  rotation(0)=angularVel(0)*timeDelta;
  rotation(1)=angularVel(1)*timeDelta;
  rotation(2)=angularVel(2)*timeDelta;
  
  /*// control towards imu's orientation
  double stiffnessangular=15;
  Quat targetAngle = ~orient;
  Vector3d angleDelta = (targetAngle*(~adjust.rotation)).toRotationVector(); // diff=target*current^-1
  angleDelta[2] = 0;  // this may not be quite right  
  angularAcc = sqr(stiffnessangular)*angleDelta -2.0*stiffnessangular*angularVel;
  angularVel += angularAcc*timeDelta;  
  rotation(0)=angularVel(0)*timeDelta;
  rotation(1)=angularVel(1)*timeDelta;
  rotation(2)=angularVel(2)*timeDelta;*/
  
  //adjust.rotation*= Quat(rotation);  
  adjust.position = offsetAcc;
  return adjust;
}

static double vel = 0.0;
static const int maxStates = 10000;
//static const int numStates = 200;
vector<Vector2d> states(maxStates);
vector<Vector2d> relativeStates(maxStates);
vector<double> phases(maxStates); // used to know where to cut the tail as it goes around the circle
// poor man's dynamic size circular queue. Just keep a head and tail index
static int queueHead = 0; 
static int queueTail = 0; 
static double timex = 0.0;
static int lastHead = 0;

static Vector2d totalPhase(0,0);
static double totalNumerator = 0;
static double totalDenominator = 0;

vector<Vector2d> queueToVector(const vector<Vector2d> &queue, int head, int tail)
{
  vector<Vector2d> result;
  for (int j = tail, i=j; i!=head; j++, i=(j%queue.size()))
    result.push_back(queue[i]);
  return result;
}

void calculatePassiveAngularFrequency()
{
  Vector2d mean(0,0);
  int numStates = 0;
  for (int j = queueTail, i=j; i!=queueHead; j++, i=(j%maxStates))
  {
    mean += states[i];
    numStates++;
  }
  if (numStates > 0)
    mean /= (double)numStates;

  // just 1 dimension for now
  double acc = 2.0*sin(3.0*timex) + random(-0.1, 0.1); // imu.linear_acceleration.y;
  double inputAngle = 3.0*timex;
  double decayRate = 0.1;
  // lossy integrator
  vel = (vel + (acc - mean[1])*timeDelta) / (1.0 + decayRate*timeDelta);
  states[queueHead] = Vector2d(vel - mean[0], acc - mean[1]);

  // increment 
  timex += timeDelta;
  double phase = atan2(states[queueHead][0], states[queueHead][1]);
  if (numStates > 1)
  {
    double phaseDiff = phase - phases[lastHead];
    while (phaseDiff > pi)
      phaseDiff -= 2.0*pi;
    while (phaseDiff < -pi)
      phaseDiff += 2.0*pi;
    phases[queueHead] = phases[lastHead] + phaseDiff;
  }
  else
    phases[queueHead] = phase;
  // now increment the tail
  // we want a robust way to find when we've completed the loop...
  while (phases[queueTail] < phases[queueHead] - 2.0*pi && queueTail != queueHead)
    queueTail = (queueTail + 1) % maxStates;
  // attempt at robustness, also work backwards, then crop based on average of both 
  {
    int tempHead = queueHead;
    while (phases[tempHead] > phases[queueHead] - 2.0*pi && tempHead != queueTail)
      tempHead = (tempHead + maxStates-1) % maxStates;
    tempHead = (tempHead + 1) % maxStates;
    int tempTail = queueTail;
    while (tempTail != tempHead && (tempTail+1)%maxStates != tempHead)
    {
      tempTail = (tempTail + 2) % maxStates;
      queueTail = (queueTail + 1) % maxStates;
    }
  }
  
  lastHead = queueHead;
  queueHead = (queueHead + 1) % maxStates; // poor man's circular queue!
    
  debugDraw->plot(queueToVector(states, queueHead, queueTail)); // should look noisy and elliptical

  
  Vector2d sumSquare(0,0);
  for (int j = queueTail, i=j; i!=queueHead; j++, i=(j%maxStates))
    sumSquare += Vector2d(sqr(states[i][0]), sqr(states[i][1]));
  totalNumerator += sumSquare[1];
  totalDenominator += sumSquare[0];
  double omega = sqrt(sumSquare[1]) / (sqrt(sumSquare[0]) + 1e-10);
  cout << "rolling omega estimate: " << omega << ", running omega estimate: " << sqrt(totalNumerator) / (sqrt(totalDenominator)+1e-10) << endl;
  
  vector<Vector2d> normalisedStates(maxStates);
  for (int j = queueTail, i=j; i!=queueHead; j++, i=(j%maxStates))
  {
    normalisedStates[i] = states[i];
    normalisedStates[i][0] *= omega;
  }
  debugDraw->plot(queueToVector(normalisedStates, queueHead, queueTail)); // this should look noisy but circularish
  
  // next, based on omega, lets plot the points 'unrotated' by the angular rate, to get a phase offset
  
  Vector2d sumUnrotated(0,0);
  double theta = -inputAngle;
  double y = normalisedStates[lastHead][0] * cos(theta) + normalisedStates[lastHead][1] * sin(theta);
  double x = normalisedStates[lastHead][0] * -sin(theta) + normalisedStates[lastHead][1] * cos(theta);
  relativeStates[lastHead] = Vector2d(x,y);
  for (int j = queueTail, i=j; i!=queueHead; j++, i=(j%maxStates))
    sumUnrotated += relativeStates[i];
  totalPhase += sumUnrotated;
  double phaseOffset = atan2(sumUnrotated[0], sumUnrotated[1]);
  double runningPhaseOffset = atan2(totalPhase[0], totalPhase[1]);
  cout << "rolling phase offset estimate: " << phaseOffset << ", running phase offset estimate: " << runningPhaseOffset << endl;
  debugDraw->plot(queueToVector(relativeStates, queueHead, queueTail)); // this should cluster around a particular phase
}