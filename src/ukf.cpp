#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>
#include <vector>
#include <fstream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {

  //initialize timestamp
	time_us_ = 0;

  // set initialization to false
  is_initialized_ = false;

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.5;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.9;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  // initialize state dimension
  n_x_ = 5;

  //initialize the augmented state
  n_aug_ = 7;

  //predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

  //initialize weights
  weights_ = VectorXd(2 * n_aug_ + 1);

  //initialize lambda-spreadng function
  lambda_ = 3 - n_aug_;

  ///* initialize all variable associated with sigma generation using augmentation
  //initialize augmented sigma matrix
  Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //initialize augmented state covariance
  P_aug_ = MatrixXd(n_aug_, n_aug_);

  //initialize augmented mean vector
  x_aug_ = VectorXd(n_aug_);

  ///*initialize all variables associated with update step for radar
  //set the measurement vector dimension for radar
  n_z_radar_ = 3;

  // initialize matrix for sigma points in measurement space for radar
  Zsig_radar_ = MatrixXd(n_z_radar_, 2 * n_aug_ + 1);

  //add measurement noise covariance matrix
  R_radar = MatrixXd(n_z_radar_, n_z_radar_);

  //initialize measurement covariance matrix S
  S_radar = MatrixXd(n_z_radar_, n_z_radar_);

  //initialize Kalman gain Matrix 
  Kalman_gain_radar = MatrixXd(n_x_, n_z_radar_);

  //initialize T matrix cross correlation
  T_cross_corr_radar = MatrixXd(n_x_, n_z_radar_);

  //create example vector for incoming radar measurement
  z_radar_ = VectorXd(n_z_radar_);

  ///* initialize variables asociated with prediction step of LIDAR/Laser
  H_ = MatrixXd(2, 5);
  H_ << 1, 0, 0, 0, 0,
	  0, 1, 0, 0, 0;
	  

  // measurement covariance matrix - laser
  R_ = MatrixXd(2,2);
  R_ << std_laspx_ * std_laspx_, 0,
	  0, std_laspy_ * std_laspy_;

  //set measaurement vector dimension for laser
  n_z_laser_ = 2;

  // initialize variable for incoming lidar measurements
  z_laser_ = VectorXd(n_z_laser_);

}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
	/**
	TODO:

	Complete this function! Make sure you switch between lidar and radar
	measurements.
	*/

	if (!is_initialized_)
	{
		
		//initialize state covariance matrix as identity
		
		P_ = P_.Identity(n_x_, n_x_);

		if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
		{
			//initialize state vector x_ for radar
			//Convert from polar to cartesian coordinates
			double rho = meas_package.raw_measurements_[0];
			double phi = meas_package.raw_measurements_[1];

			double px = rho * cos(phi);
			double py = rho * sin(phi);


			x_ << px,
				py,
				0,
				0,
				0;

		}

		else if (meas_package.sensor_type_ == MeasurementPackage::LASER)
		{
			//initialize state vector x for laser
			x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0, 0, 0;
		}
		time_us_ = meas_package.timestamp_;
		is_initialized_ = true;
		//done initializing.No need to predict or update
		return;
	}

	

	// start Update step for Lidar OR Radar
	if (use_laser_)
	{
		if (meas_package.sensor_type_ == MeasurementPackage::LASER)
		{
			//initialize delta_t
			double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;

			/*std::cout << "delta_t" << std::endl;
			std::cout << delta_t << std::endl;*/
			// reinitialize timestamp time_us_
			time_us_ = meas_package.timestamp_;

			// start prediction step
			Prediction(delta_t);

			//start update step
			UpdateLidar(meas_package);
		}
	}
	if (use_radar_)
	{
		if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
		{

			//initialize delta_t
			double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;

			/*std::cout << "delta_t" << std::endl;
			std::cout << delta_t << std::endl;*/
			// reassign latest value of timestamp  to time_us_
			time_us_ = meas_package.timestamp_;

			// start prediction step
			Prediction(delta_t);

			//start update step
			UpdateRadar(meas_package);
		}
		
	}

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */


void UKF::Prediction(double delta_t){
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

	///*Let's start with generating the sigma points
	// create augmented state matrix
	x_aug_.head(5) = x_;
	x_aug_(5) = 0;
	x_aug_(6) = 0;
	
	//create augmented covariance matrix
	P_aug_.fill(0.0);
	P_aug_.topLeftCorner(5, 5) = P_;
	P_aug_(5, 5) = std_a_ * std_a_;
	P_aug_(6, 6) = std_yawdd_ * std_yawdd_;

	////create square root matrix for P_aug_
	//MatrixXd L = P_aug_.llt().matrixL();
	Eigen::LLT<MatrixXd> lltOfPaug(P_aug_);

	if (lltOfPaug.info() == Eigen::NumericalIssue) {

		cout << "LLT failed!" << endl;
		throw range_error("LLT failed");
	}

	MatrixXd L = lltOfPaug.matrixL();

	//create augmented sigma points
	Xsig_aug_.col(0) = x_aug_;
	for (int ii = 0; ii < n_aug_; ++ii)
	{
		Xsig_aug_.col(ii + 1) = x_aug_ + sqrt(lambda_ + n_aug_) * L.col(ii);
		Xsig_aug_.col(ii + 1 +n_aug_) = x_aug_ - sqrt(lambda_ + n_aug_) * L.col(ii);
	}

	///* Let's predict the sigma points
	//predict sigma points
	for (int i = 0; i < 2 * n_aug_ + 1; ++i)
	{
		//extract values for better readability
		double p_x = Xsig_aug_(0, i);
		double p_y = Xsig_aug_(1, i);
		double v = Xsig_aug_(2, i);
		double yaw = Xsig_aug_(3, i);
		double yawd = Xsig_aug_(4, i);
		double nu_a = Xsig_aug_(5, i);
		double nu_yawdd = Xsig_aug_(6, i);

		//predicted state values
		double px_p, py_p;

		//avoid division by zero
		//These are equations given in L7.20
		if (fabs(yawd) > 0.001) {
			px_p = p_x + v / yawd * (sin(yaw + yawd*delta_t) - sin(yaw));
			py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd*delta_t));
		}
		else {
			px_p = p_x + v*delta_t*cos(yaw);
			py_p = p_y + v*delta_t*sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd*delta_t;
		double yawd_p = yawd;

		//add noise.Equations given in Lesson7.20
		px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
		py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
		v_p = v_p + nu_a*delta_t;

		yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
		yawd_p = yawd_p + nu_yawdd*delta_t;

		//write predicted sigma point into right column.We will have 5 * 15 matrix Xsig_pred
		Xsig_pred_(0, i) = px_p;
		Xsig_pred_(1, i) = py_p;
		Xsig_pred_(2, i) = v_p;
		Xsig_pred_(3, i) = yaw_p;
		Xsig_pred_(4, i) = yawd_p;

	}
		///* Let's start prediction of mean and covariance matrix
		//set the weights according to equation in L7.23
		weights_(0) = lambda_ / (lambda_ + n_aug_);
		for (int ii = 1; ii < 2 * n_aug_ + 1; ++ii)
		{
			weights_(ii) = 0.5/(lambda_ + n_aug_);
		}
		
		//predicted state mean
		x_.fill(0.0);
		for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  //iterate over sigma points
			x_ = x_ + weights_(i) * Xsig_pred_.col(i);
		}

		/*std::cout << "x_pred" << std::endl;
		std::cout << x_ << std::endl;*/
		//predicted state covariance matrix
		P_.fill(0.0);
		for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

												   // state difference
			VectorXd x_diff = Xsig_pred_.col(i) - x_;

			//angle normalization
			while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
			while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

			P_ = P_ + weights_(i) * x_diff * x_diff.transpose();

		}
		/*std::cout << "P_pred" << std::endl;
		std::cout << P_ << std::endl;*/
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

	// store the incoming laser mesaurements
	z_laser_ << meas_package.raw_measurements_[0],
		meas_package.raw_measurements_[1];
		
	//start the update process
	VectorXd z_pred = H_ * x_;
	VectorXd y = z_laser_ - z_pred;
	MatrixXd Ht = H_.transpose();
	MatrixXd S = (H_ * P_ * Ht) + R_;
	MatrixXd Si = S.inverse();
	MatrixXd K = P_ * Ht * Si;

	// new estimate
	x_ = x_ + (K * y);
	long x_size = x_.size();
	MatrixXd I = MatrixXd::Identity(x_size, x_size);
	P_ = (I - K * H_) * P_;

	//calculate NIS for laser
	//residual
	VectorXd z_diff_laser = z_laser_ - z_pred;

	//calculate NIS for laser
	double NIS_laser_ = z_diff_laser.transpose() * Si * z_diff_laser;

	//store NIS radar measurements
	//NIS_vector_laser_.push_back(NIS_laser_);

	/*std::ofstream myLaserFile("C:\\Users\\saurabh B\\Documents\\Laser.txt", std::ios_base::out | std::ios_base::app | std::ios::binary);
	myLaserFile << NIS_laser_ << std::endl;
	myLaserFile.close();*/

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  /* predict measurement for radar.Get measurement mean zsig_radar and
	/predicted measurement covariance matrix S_radar
	*/
  //transform sigma points into measurement space
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

											   // extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);
		double v = Xsig_pred_(2, i);
		double yaw = Xsig_pred_(3, i);

		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

		// measurement model
		Zsig_radar_ (0, i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig_radar_(1, i) = atan2(p_y, p_x);                                //phi
		Zsig_radar_(2, i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
	}

	//mean predicted measurement
	VectorXd z_pred_radar = VectorXd(n_z_radar_);
	z_pred_radar.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {
		z_pred_radar = z_pred_radar + weights_(i) * Zsig_radar_.col(i);
	}

	
	S_radar.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
											   //residual
		VectorXd z_diff = Zsig_radar_.col(i) - z_pred_radar;

		//angle normalization
		while (z_diff(1) > M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1) < -M_PI) z_diff(1) += 2.*M_PI;

		S_radar = S_radar + weights_(i) * z_diff * z_diff.transpose();

	}

		R_radar << std_radr_*std_radr_, 0, 0,
			0, std_radphi_*std_radphi_, 0,
			0, 0, std_radrd_*std_radrd_;
		S_radar =S_radar + R_radar;
	

	///* start with update step to get x_ and covariance P_
	//calculate cross correlation matrix
	T_cross_corr_radar.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

		//residual
		VectorXd z_diff = Zsig_radar_.col(i) - z_pred_radar;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

		T_cross_corr_radar = T_cross_corr_radar + weights_(i) * x_diff * z_diff.transpose();
	}

	//Kalman gain K;
	 Kalman_gain_radar = T_cross_corr_radar * S_radar.inverse();

	 /*std::cout << "kalman" << std::endl;
	 std::cout << Kalman_gain_radar << std::endl;*/


	z_radar_ << meas_package.raw_measurements_[0],
		meas_package.raw_measurements_[1],
		meas_package.raw_measurements_[2];

	//residual
	VectorXd z_diff = z_radar_ - z_pred_radar;


	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

	//update state mean and covariance matrix for radar
	x_ = x_ + Kalman_gain_radar * z_diff;
	P_ = P_ - Kalman_gain_radar * S_radar * Kalman_gain_radar.transpose();

	/*std::cout << "x_update" << std::endl;
	std::cout << x_ << std::endl;*/


	//calculate NIS for radar
	 double NIS_radar_ = z_diff.transpose() * S_radar.inverse() * z_diff;

	/*std::ofstream myRadarFile("C:\\Users\\saurabh B\\Documents\\Radar.txt", std::ios_base::out | std::ios_base::app |std::ios::binary);
	myRadarFile << NIS_radar_ << std::endl;
	myRadarFile.close();*/
}

