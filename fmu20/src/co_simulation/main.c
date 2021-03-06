/* -------------------------------------------------------------------------
 * main.c
 *
 * Author: Orthogonal 
 * -------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fmi2.h"
#include "sim_support.h"

#include <rtai_lxrt.h>

FMU fmu; // the fmu to simulate

static long overrun=0;
static long totalrun=0;

int get_system_output(char *cmd, char *output, int size) {
    FILE *fp=NULL;
    fp = popen(cmd, "r");
    if (fp) {
        if(fgets(output, size, fp) != NULL) {
            if(output[strlen(output)-1] == '\n')
                output[strlen(output)-1] = '\0';
        }
        pclose(fp);
    }
    return 0;
}



double compute_time(struct timespec start, struct timespec end,float h) {
    double diffSec, ratio;
    diffSec = (double)(end.tv_sec-start.tv_sec)*1000000  + (double)(end.tv_nsec - start.tv_nsec)/1000;
    ratio = diffSec/(h*1000000);  // in us

    totalrun++;
    if ( ratio>=1.0)
        overrun++;

    printf("Check 1 sample step: done time window=[%.6lf us], ratio=[%.2lf%]. Total#=[%d], Overrun#=[%d]\n", diffSec, ratio, totalrun, overrun);
    return diffSec;
}


// simulate the given FMU from tStart = 0 to tEnd.
static int simulate(FMU* fmu, double tEnd, double h, fmi2Boolean loggingOn, char separator,
                    int nCategories, char **categories) {


    char *tmppstr=(char*)malloc(512);
    char cmd[128];
    strcpy( cmd, "cat /tmp/modelDescription.xml   | grep stepSize  | awk -F'=' '{print $2}' |  awk -F'\"' '{print $2}'" );
    get_system_output ( cmd, tmppstr,  63);
    sscanf( tmppstr, "%le", &h);
    strcpy( cmd, "cat /tmp/modelDescription.xml   | grep stopTime  | awk -F'=' '{print $2}' |  awk -F'\"' '{print $2}'" );
    get_system_output ( cmd, tmppstr,  63);
    sscanf( tmppstr, "%le", &tEnd);



    int outputrow_step=1;
    int outputlog_step=1;
    double tmpd = tEnd/h;
     
    while ( (tmpd/outputrow_step)>1200 ){
        outputrow_step = outputrow_step*10;
        if (outputrow_step>1000000000){
            printf ( "Invalid step and end time, exit \n" );
            exit(-1);
        }
    }
    while ( (tmpd/outputlog_step)>200 ){
        outputlog_step = outputlog_step*10;
        if (outputlog_step>1000000000){
            printf ( "Invalid step and end time, exit \n" );
            exit(-1);
        }
    }

    printf( "===== outputrow_step=%d outputlog_step=%d\n", outputrow_step, outputlog_step);



    double time;
    double tStart = 0;                      // start time
    const char *guid;                       // global unique id of the fmu
    const char *instanceName;               // instance name
    fmi2Component c;                        // instance of the fmu
    fmi2Status fmi2Flag;                    // return code of the fmu functions
    char *fmuResourceLocation = getTempResourcesLocation(); // path to the fmu resources as URL, "file://C:\QTronic\sales"
    fmi2Boolean visible = fmi2False;        // no simulator user interface

    fmi2CallbackFunctions callbacks = {fmuLogger, calloc, free, NULL, fmu};  // called by the model during simulation
    ModelDescription* md;                      // handle to the parsed XML file
    fmi2Boolean toleranceDefined = fmi2False;  // true if model description define tolerance
    fmi2Real tolerance = 0;                    // used in setting up the experiment
    ValueStatus vs = valueMissing;
    int nSteps = 0;
    double hh = h;
    Element *defaultExp;
    FILE* file;

    // instantiate the fmu
    md = fmu->modelDescription;
    guid = getAttributeValue((Element *)md, att_guid);
    instanceName = getAttributeValue((Element *)getCoSimulation(md), att_modelIdentifier);
    c = fmu->instantiate(instanceName, fmi2CoSimulation, guid, fmuResourceLocation,
                         &callbacks, visible, loggingOn);
    free(fmuResourceLocation);
    if (!c) return error("could not instantiate model");

    if (nCategories > 0) {
        fmi2Flag = fmu->setDebugLogging(c, fmi2True, nCategories, categories);
        if (fmi2Flag > fmi2Warning) {
            return error("could not initialize model; failed FMI set debug logging");
        }
    }

    defaultExp = getDefaultExperiment(md);
    if (defaultExp) tolerance = getAttributeDouble(defaultExp, att_tolerance, &vs);
    if (vs == valueDefined) {
        toleranceDefined = fmi2True;
    }



    fmi2Flag = fmu->setupExperiment(c, toleranceDefined, tolerance, tStart, fmi2True, tEnd);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI setup experiment");
    }

    fmi2Flag = fmu->enterInitializationMode(c);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI enter initialization mode");
    }
    fmi2Flag = fmu->exitInitializationMode(c);
    if (fmi2Flag > fmi2Warning) {
        return error("could not initialize model; failed FMI exit initialization mode");
    }

    // open result file
    if (!(file = fopen(RESULT_FILE, "w"))) {
        printf("could not write %s because:\n", RESULT_FILE);
        printf("    %s\n", strerror(errno));
        return 0; // failure
    }

    // output solution for time t0
    outputRow(fmu, c, tStart, file, separator, fmi2True);  // output column names
    outputRow(fmu, c, tStart, file, separator, fmi2False); // output values


    // ================= added by jiacheng on 0323 ==================================
    struct timespec current,org;
    long diffSec;
    fmi2Status status;

    unsigned long overrun;

    RT_TASK *task=0;

    int period=0 ;

    unsigned long cnt=0;

    //signal(SIGUSR1,endRpcServer);
    //signal(SIGTERM,endHandler);
    //signal(SIGALRM,endHandler);

    if(!(task=rt_task_init_schmod(nam2num("TEST"),0,0, 0,SCHED_FIFO,0x0f))) {
        printf("Can't initial the task\n");
        exit(1);
    }

    mlockall(MCL_CURRENT|MCL_FUTURE);

    period=start_rt_timer(nano2count(h*1000000000));

    rt_make_hard_real_time();

    rt_task_make_periodic(task,rt_get_time()+period*10,period);

    //clock_gettime(CLOCK_MONOTONIC, &org);
    //printf( "==== start loop:  stopTime[%ld], stepTime[%ld] %d, pid=%d\n", this->stopTime_us, this->stepTime_us,  this->runStatus, getpid()) ;

    cnt = 0;



    time = tStart;
    while (time < tEnd) {

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);


        // check not to pass over end time
        if (h > tEnd - time) {
            hh = tEnd - time;
        }

        fmi2Flag = fmu->doStep(c, time, hh, fmi2True);
        if (fmi2Flag == fmi2Discard) {
            fmi2Boolean b;
            // check if model requests to end simulation
            if (fmi2OK != fmu->getBooleanStatus(c, fmi2Terminated, &b)) {
                return error("could not complete simulation of the model. getBooleanStatus return other than fmi2OK");
            }
            if (b == fmi2True) {
                return error("the model requested to end the simulation");
            }
            return error("could not complete simulation of the model");
        }
        if (fmi2Flag != fmi2OK) return error("could not complete simulation of the model");
        time += hh;

        if (nSteps%outputrow_step==0 )
            outputRow(fmu, c, time, file, separator, fmi2False); // output values for this step

        nSteps++;


        clock_gettime(CLOCK_MONOTONIC, &end);
        if (nSteps%outputlog_step==0 ) {       //
            compute_time(start, end, h);
            printf( "==== Fixed time step:  %f of %f done.\n", time, tEnd);
        }


        rt_task_wait_period();

    }



    rt_make_soft_real_time();

    stop_rt_timer();

    rt_task_delete(task);



    // end simulation
    fmu->terminate(c);
    fmu->freeInstance(c);
    fclose(file);

    // print simulation summary
    printf("Simulation from %g to %g terminated successful\n", tStart, tEnd);
    printf("  steps ............ %d\n", nSteps-1);
    printf("  fixed step size .. %g\n", h);
    return 1; // success

}





int main(int argc, char *argv[]) {

    const char* fmuFileName;
    int i;

    char cmd[512];
    char *tmptime=(char*)malloc(64);
    float l_stime, l_etime, l_step;


    // parse command line arguments and load the FMU
    // default arguments value
    double tEnd = 1.0;
    double h=0.1;

    int loggingOn = 0;
    char csv_separator = ',';
    char **categories = NULL;
    int nCategories = 0;

    parseArguments(argc, argv, &fmuFileName, &tEnd, &h, &loggingOn, &csv_separator, &nCategories, &categories);
    loadFMU(fmuFileName);


    // run the simulation
    printf("FMU Simulator: run '%s' from t=0..%g with step size h=%g, loggingOn=%d, csv separator='%c' ",
           fmuFileName, tEnd, h, loggingOn, csv_separator);
    printf("log categories={ ");
    for (i = 0; i < nCategories; i++) printf("%s ", categories[i]);
    printf("}\n");

    simulate(&fmu, tEnd, h, loggingOn, csv_separator, nCategories, categories);
    printf("CSV file '%s' written\n", RESULT_FILE);

    // release FMU
#if WINDOWS
    FreeLibrary(fmu.dllHandle);
#else /* WINDOWS */
    dlclose(fmu.dllHandle);
#endif /* WINDOWS */
    freeModelDescription(fmu.modelDescription);
    if (categories) free(categories);

    // delete temp files obtained by unzipping the FMU
    deleteUnzippedFiles();

    return EXIT_SUCCESS;
}
