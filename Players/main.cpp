/****************************************************************************
*                                                                           *
*   Nite 1.3 - Players Sample                                               *
*                                                                           *
*   Author:     Oz Magal                                                    *
*                                                                           *
****************************************************************************/

/****************************************************************************
*                                                                           *
*   Nite 1.3	                                                            *
*   Copyright (C) 2006 PrimeSense Ltd. All Rights Reserved.                 *
*                                                                           *
*   This file has been provided pursuant to a License Agreement containing  *
*   restrictions on its use. This data contains valuable trade secrets      *
*   and proprietary information of PrimeSense Ltd. and is protected by law. *
*                                                                           *
****************************************************************************/

#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>
#include "SceneDrawer.h"
#include <stdlib.h>
//#include <shape.h>
#define _USE_MATH_DEFINES
#include <math.h>

xn::Context g_Context;
xn::DepthGenerator g_DepthGenerator;
xn::UserGenerator g_UserGenerator;
xn::Recorder* g_pRecorder;

XnUserID g_nPlayer = 0;
XnBool g_bCalibrated = FALSE;

//void join(XnUserID , XnSkeletonJoint , XnSkeletonJoint);
//XXX: added lines 37-39
XnPoint3D oTorso; //original torso joint location

int window1, window2;

#ifdef USE_GLUT
#include <GL/glut.h>
#else
//#include "opengles.h"
#include "kbhit.h"
#endif
//#include "signal_catch.h"

GLdouble r = -1.0; //


#ifndef USE_GLUT
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;
#endif

#define GL_WIN_SIZE_X 720
#define GL_WIN_SIZE_Y 480
#define START_CAPTURE_CHECK_RC(rc, what)												\
	if (nRetVal != XN_STATUS_OK)														\
{																					\
	printf("Failed to %s: %s\n", what, xnGetStatusString(rc));				\
	StopCapture();															\
	return ;																	\
}

XnBool g_bPause = false;
XnBool g_bRecord = false;

XnBool g_bQuit = false;



void StopCapture()
{
	g_bRecord = false;
	if (g_pRecorder != NULL)
	{
		g_pRecorder->RemoveNodeFromRecording(g_DepthGenerator);
		g_pRecorder->Unref();
		delete g_pRecorder;
	}
	g_pRecorder = NULL;
}

void CleanupExit()
{
	if (g_pRecorder)
		g_pRecorder->RemoveNodeFromRecording(g_DepthGenerator);
	StopCapture();
	g_Context.Shutdown();
	exit (1);
}

void StartCapture()
{
	char recordFile[256] = {0};
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
        XnUInt32 size;
        xnOSStrFormat(recordFile, sizeof(recordFile)-1, &size,
                 "%d_%02d_%02d[%02d_%02d_%02d].oni",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

	if (g_pRecorder != NULL)
	{
		StopCapture();
	}

	XnStatus nRetVal = XN_STATUS_OK;
	g_pRecorder = new xn::Recorder;

	g_Context.CreateAnyProductionTree(XN_NODE_TYPE_RECORDER, NULL, *g_pRecorder);
	START_CAPTURE_CHECK_RC(nRetVal, "Create recorder");

	nRetVal = g_pRecorder->SetDestination(XN_RECORD_MEDIUM_FILE, recordFile);
	START_CAPTURE_CHECK_RC(nRetVal, "set destination");
	nRetVal = g_pRecorder->AddNodeToRecording(g_DepthGenerator, XN_CODEC_16Z_EMB_TABLES);
	START_CAPTURE_CHECK_RC(nRetVal, "add node");
	g_bRecord = true;
}

XnBool AssignPlayer(XnUserID user)
{
	if (g_nPlayer != 0)
		return FALSE;

	XnPoint3D com;
	g_UserGenerator.GetCoM(user, com);
	if (com.Z == 0)
		return FALSE;

	printf("Matching for existing calibration\n");
	g_UserGenerator.GetSkeletonCap().LoadCalibrationData(user, 0);
	g_UserGenerator.GetSkeletonCap().StartTracking(user);
	g_nPlayer = user;
	return TRUE;

}
void XN_CALLBACK_TYPE NewUser(xn::UserGenerator& generator, XnUserID user, void* pCookie)
{
	if (!g_bCalibrated) // check on player0 is enough
	{
		printf("Look for pose\n");
		g_UserGenerator.GetPoseDetectionCap().StartPoseDetection("Psi", user);
		return;
	}

	AssignPlayer(user);
 	/*if (g_nPlayer == 0)
 	{
 		printf("Assigned user\n");
 		g_UserGenerator.GetSkeletonCap().LoadCalibrationData(user, 0);
 		g_UserGenerator.GetSkeletonCap().StartTracking(user);
 		g_nPlayer = user;
 	}*/
}
void FindPlayer()
{
	if (g_nPlayer != 0)
	{
		return;
	}
	XnUserID aUsers[20];
	XnUInt16 nUsers = 20;
	g_UserGenerator.GetUsers(aUsers, nUsers);

	for (int i = 0; i < nUsers; ++i)
	{
		if (AssignPlayer(aUsers[i]))
			return;
	}
}
void LostPlayer()
{
	g_nPlayer = 0;
	FindPlayer();
}
void XN_CALLBACK_TYPE LostUser(xn::UserGenerator& generator, XnUserID user, void* pCookie)
{
	printf("Lost user %d\n", user);
	if (g_nPlayer == user)
	{
		LostPlayer();
	}
}
void XN_CALLBACK_TYPE PoseDetected(xn::PoseDetectionCapability& pose, const XnChar* strPose, XnUserID user, void* cxt)
{
	printf("Found pose \"%s\" for user %d\n", strPose, user);
	g_UserGenerator.GetSkeletonCap().RequestCalibration(user, TRUE);
	g_UserGenerator.GetPoseDetectionCap().StopPoseDetection(user);
}

void XN_CALLBACK_TYPE CalibrationStarted(xn::SkeletonCapability& skeleton, XnUserID user, void* cxt)
{
	printf("Calibration started\n");
}

void XN_CALLBACK_TYPE CalibrationEnded(xn::SkeletonCapability& skeleton, XnUserID user, XnBool bSuccess, void* cxt)
{
	printf("Calibration done [%d] %ssuccessfully\n", user, bSuccess?"":"un");
	if (bSuccess)
	{
		if (!g_bCalibrated)
		{
			g_UserGenerator.GetSkeletonCap().SaveCalibrationData(user, 0);
			g_nPlayer = user;
			g_UserGenerator.GetSkeletonCap().StartTracking(user);
			g_bCalibrated = TRUE;
		}
        	/*/XXX: added lines 215-228
		XnSkeletonJointPosition torso, neck;
		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(user, XN_SKEL_TORSO, torso);
		g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(user, XN_SKEL_NECK, neck);
		XnPoint3D pt[2];
		oTorso = torso.position;
		//pt[0] = oTorso.position;
		pt[1] = neck.position;
		GLdouble dx, dy, dz, size;
		dx = pt[1].X - oTorso.X;//pt[0].X;
		dy = pt[1].Y - oTorso.Y;//pt[0].Y;
  		dz = pt[1].Z - oTorso.Z;//pt[0].Z;
		size = sqrt(dx*dx + dy*dy + dz*dz);
		r = (GLdouble) size/4.0;

		printf("%f\n", torso.fConfidence);*/

		XnUserID aUsers[10];
		XnUInt16 nUsers = 10;
		g_UserGenerator.GetUsers(aUsers, nUsers);
		for (int i = 0; i < nUsers; ++i)
			g_UserGenerator.GetPoseDetectionCap().StopPoseDetection(aUsers[i]);
	}
}


void DrawProjectivePoints(XnPoint3D& ptIn, int width, double r, double g, double b)
{
	static XnFloat pt[3];

	pt[0] = ptIn.X;
	pt[1] = ptIn.Y;
	pt[2] = 0;
	glColor4f(r,
		g,
		b,
		1.0f);
	glPointSize(width);
	glVertexPointer(3, GL_FLOAT, 0, pt);
	glDrawArrays(GL_POINTS, 0, 1);

	glFlush();

}

GLdouble CalcAngle(GLdouble dx, GLdouble dy) {
    GLdouble theta = atan(dx/dy) * (180/M_PI);
    if (dy > 0 && dx > 0) { return theta; }
    if (dy > 0) { return 360 + theta; }
    return 180 + theta;

    if (dx < 0 && dy < 0) {
        return 180 + theta;
    }
    if (dy > 0 && dx < 0) {
        return 180 + theta;
    }
    return theta;
}


void join(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2, GLdouble length) {
    XnSkeletonJointPosition joint1, joint2;
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);
    if(joint1.fConfidence < 0.5 || joint2.fConfidence < 0.5) { return; }

    
    XnPoint3D pt[2];
    pt[0] = joint1.position;
    pt[1] = joint2.position;
    GLdouble dx, dy, dz, yrot, zrot, size;
    dx = (pt[1].X - pt[0].X)/100.0;
    dy = (pt[1].Y - pt[0].Y)/100.0;
    dz = (pt[1].Z - pt[0].Z)/100.0;
    size = sqrt(dx*dx + dy*dy + dz*dz);
    if(length == 0) { length = size; }
    dx = length * (dx/size);
    dy = length * (dy/size);
    dz = length * (dz/size);
    size = sqrt(dx*dx + dy*dy + dz*dz);
    yrot = atan2(dz, dx) * (180/M_PI);
    zrot = asin(dy/size) * (180/M_PI);
    
    
    glPushMatrix();
	glTranslated(pt[0].X/100.0, pt[0].Y/100.0, pt[0].Z/100.0);
    if(dx != 0.0 || dz != 0.0) {
        glRotated(yrot, 0.0, -1.0, 0.0);
    }
    glRotated(zrot, 0.0, 0.0, 1.0);
    glRotated(90.0, 0.0, 1.0, 0.0);
    gluCylinder(gluNewQuadric(), 0.5, 0.5, length, 32, 1);
    glPopMatrix();
}

void drawHead(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2, GLdouble length) {
    XnSkeletonJointPosition joint1, joint2;
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
    g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);
    if(joint1.fConfidence < 0.5 || joint2.fConfidence < 0.5) { return; }
    XnPoint3D pt[2];
    pt[0] = joint1.position;
    pt[1] = joint2.position;
    GLdouble dx, dy, dz, yrot, zrot, size;
    dx = (pt[1].X - pt[0].X)/100.0;
    dy = (pt[1].Y - pt[0].Y)/100.0;
    dz = (pt[1].Z - pt[0].Z)/100.0;
    size = sqrt(dx*dx + dy*dy + dz*dz);
    if(length == 0) { length = size; }
    dx = length * (dx/size);
    dy = length * (dy/size);
    dz = length * (dz/size);
    size = sqrt(dx*dx + dy*dy + dz*dz);
    yrot = atan2(dz, dx) * (180/M_PI);
    zrot = asin(dy/size) * (180/M_PI);
    
    
    glPushMatrix();
	glTranslated(pt[1].X/100.0, pt[1].Y/100.0, pt[1].Z/100.0);
    if(dx != 0.0 || dz != 0.0) {
        glRotated(yrot, 0.0, -1.0, 0.0);
    }
    glRotated(zrot, 0.0, 0.0, 1.0);
    glRotated(90.0, 0.0, 1.0, 0.0);
    gluSphere(gluNewQuadric(),1.0, 15, 15);
    glPopMatrix();
}    





void display(XnUserID player) {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    glTranslated(0.0, 0.0, -80.0);
    
    if(player != 0) {
  
	glColor3f(1.0f,1.0f,1.0f);	
        drawHead(player, XN_SKEL_NECK, XN_SKEL_HEAD, 0.0);
        glColor3f(1.0f,0.0f,0.0f);
	glPushMatrix();
	join(player, XN_SKEL_TORSO, XN_SKEL_NECK, 0.0);
	glPushMatrix();
	glColor3f(0.0f,1.0f,0.0f);
	join(player, XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW, 0.0);
	glColor3f(0.0f,0.0f,1.0f);
	join(player, XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND, 0.0);
	glPopMatrix();
	glColor3f(0.0f,0.0f,1.0f);
	join(player, XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW, 0.0);
	glColor3f(0.0f,1.0f,0.0f);
	join(player, XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND, 0.0);
	glPopMatrix();
	glPushMatrix();
	glColor3f(0.0f,1.0f,0.0f);
	join(player, XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE, 0.0);
	glColor3f(0.0f,0.0f,1.0f);
	join(player, XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT, 0.0);
	glPopMatrix();
	glColor3f(0.0f,0.0f,1.0f);
	join(player, XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE, 0.0);
	glColor3f(0.0f,1.0f,0.0f);
	join(player, XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT, 0.0);
        glPopMatrix();
    }
    glFlush();
    glutSwapBuffers();
}




// this function is called each frame
void glutDisplay (void)
{
    xn::SceneMetaData sceneMD;
    xn::DepthMetaData depthMD;
    g_DepthGenerator.GetMetaData(depthMD);
    
    if (!g_bPause)
    {
        // Read next available data
        g_Context.WaitAndUpdateAll();
    }

        // Process the data
        //DRAW
        g_DepthGenerator.GetMetaData(depthMD);
        g_UserGenerator.GetUserPixels(0, sceneMD);
        display(g_nPlayer);
        if (g_nPlayer != 0)
        {
            XnPoint3D com;
            g_UserGenerator.GetCoM(g_nPlayer, com);
            if (com.Z == 0)
            {
                g_nPlayer = 0;
                FindPlayer();
            }
        }
}


void glutIdle (void)
{
	if (g_bQuit) {
		CleanupExit();
	}

	// Display the frame
	glutPostRedisplay();
}

void glutKeyboard (unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		CleanupExit();
	case'p':
		g_bPause = !g_bPause;
		break;
	case 'k':
		if (g_pRecorder == NULL)
			StartCapture();
		else
			StopCapture();
		printf("Record turned %s\n", g_pRecorder ? "on" : "off");
		break;
	}
}




void reshape(int w, int h) {
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (GLfloat) w / (GLfloat) h, 0.1, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}



void init(void) {
    glShadeModel(GL_FLAT);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClearDepth(1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}


#define SAMPLE_XML_PATH "../../Data/Sample-User.xml"

#define CHECK_RC(rc, what) \
    if (rc != XN_STATUS_OK) \
    { \
        printf("%s failed: %s\n", what, xnGetStatusString(rc)); \
        return rc; \
    }



void display1(void) {
    glutSetWindow(window1);
    glutDisplay();
    glutSetWindow(window2);
    glutDisplay();
}

void glutDisplay2(void) {
	glutDisplay();
}	

int main(int argc, char** argv) {

    XnStatus rc = XN_STATUS_OK;

    rc = g_Context.InitFromXmlFile(SAMPLE_XML_PATH);
    CHECK_RC(rc, "InitFromXml");

    rc = g_Context.FindExistingNode(XN_NODE_TYPE_DEPTH, g_DepthGenerator);
    CHECK_RC(rc, "Find depth generator");
    rc = g_Context.FindExistingNode(XN_NODE_TYPE_USER, g_UserGenerator);
    CHECK_RC(rc, "Find user generator");

    if (!g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON) ||
        !g_UserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
    {
        printf("User generator doesn't support either skeleton or pose detection.\n");
        return XN_STATUS_ERROR;
    }
    g_UserGenerator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

    rc = g_Context.StartGeneratingAll();
    CHECK_RC(rc, "StartGenerating");

    XnCallbackHandle hUserCBs, hCalibrationCBs, hPoseCBs;
    g_UserGenerator.RegisterUserCallbacks(NewUser, LostUser, NULL, hUserCBs);
    g_UserGenerator.GetSkeletonCap().RegisterCalibrationCallbacks(CalibrationStarted, CalibrationEnded, NULL, hCalibrationCBs);
    g_UserGenerator.GetPoseDetectionCap().RegisterToPoseCallbacks(PoseDetected, NULL, NULL, hPoseCBs);


    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(100, 100);
    window1 = glutCreateWindow("test 1");
    init();
    glutDisplayFunc(glutDisplay);
    glutReshapeFunc(reshape);
//    glutIdleFunc(glutIdle);

    window2 = glutCreateWindow("test 2");
//    init();
    glutDisplayFunc(glutDisplay2);
    glutReshapeFunc(reshape);
    glutIdleFunc(glutIdle);
//    glutKeyboardFunc(glutKeyboard);
    glutMainLoop();
    return 0;
}



