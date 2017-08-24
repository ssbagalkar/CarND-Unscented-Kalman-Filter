#include <iostream>
#include "tools.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
	const vector<VectorXd> &ground_truth) {
	/**
	TODO:
	  * Calculate the RMSE here.
	*/
	VectorXd temp(4);
	temp << 0, 0, 0, 0;

	if (estimations.size() != ground_truth.size()
		|| estimations.size() == 0)
	{
		cout << "invalid estimation or ground truth" << endl;
	}

	//accumulate squared residuals
	for (unsigned int i = 0; i < estimations.size(); ++i)
	{
		VectorXd residual = estimations[i] - ground_truth[i];

		//coefficient-wise multipilcation
		residual = residual.array() * residual.array();
		temp += residual;
	}

	//calculate mean
	temp = temp / estimations.size();

	//calculate squared root
	temp = temp.array().sqrt();
	return temp;
}