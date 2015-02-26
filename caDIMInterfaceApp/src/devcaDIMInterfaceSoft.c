/* devcaDIMInterfaceSoft.c - integrates server and client */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbLock.h"
#include "aiRecord.h"
#include "aoRecord.h"
#include "assert.h"

#include "epicsExport.h"

/*DIM specific */
#include "dis.h"
#include "dic.h"
#include "dbScan.h"

/* devcaDIMInterface */
#include "ptools.h"
#include "disassembleString.h"
#include "devcaDIMInterfaceSoft.h"

/*Definition of identifiers used in the inputOutput string.*/
#include "devcaDIMInterfaceSoft_preProcessorStatements.c"

/*Dim_call_back is the user routine for a subscribed service as well
 * as for a received command at the moment.*/
void dimCallback();

/*To compare data to noLink in callback, data has to be cast to the same datatype as nolink.
 * By chance, it is always possible to have exactly the same data content as the content
 * specified by nolink, though.*/

#define DIMNOLINK "NO LINK!!!"
char *nolink = NULL;

/*DIM declarations*/
#define MAX_DIMRecords 1000
#define MAXIMUMDIMDNSADDRESSLENGTH 254 /* see utilities.c */

/*
 * global variables
 */
static int recordIndex = 0;
static bool dimServerDnsAddressSet = false;
static bool dimClientDnsAddressSet = false;
static bool dimServerDnsPortSet = false;
static bool dimClientDnsPortSet = false;
static char* serverName = NULL;
/*  Integrate variables belonging together in a struct, put each struct in an array.*/
static struct recordVariables** recordVariablesArray = NULL;

/*variables for a command callback routine, currently in development/test stage*/
void serverCmndCallback();
void getDimFormatCallback(long *tag,  void *data,  int *size);

static long report();
static long init_record();
static long process();
struct
{
   long		number;
   DEVSUPFUN	report;
   DEVSUPFUN	init;
   DEVSUPFUN	init_record;
   DEVSUPFUN	get_ioint_info;
   DEVSUPFUN	process;
   DEVSUPFUN	special_lincov;
}
devcaDIMInterfaceSoft = {
                           6,
                           report,
                           NULL,
                           init_record,
                           NULL,
                           process,
                           NULL
                        };

epicsExportAddress(dset,devcaDIMInterfaceSoft);

/*report is not necessary, just implemented for testing purposes*/
static long report (int level)
{
   message(stdout,NULL,0,NULL,NULL,"Report; Number of DIM Records: %i \n", recordIndex);
   return (0);
}

static long init_record(struct dbCommon	*pRecord)
{
   /*
    * determine type of record 
    * 	(dbCommon->dbRecordType->name (pRecord->rdes->name))
    */
   int recordTypeIndex = determineRecordType(pRecord);
   /*
    * TODO: what happens here? (EPICS wise)
    */
   switch(recordTypeIndex)
   {
   case DIMaiRecord:
      if(recGblInitConstantLink(&(((aiRecord*)pRecord)->inp),DBF_DOUBLE,&(((aiRecord*)pRecord)->val)))
      {
         pRecord->udf = FALSE;
      }
      break;
   case DIMaoRecord:
      if(recGblInitConstantLink(&(((aoRecord*)pRecord)->out),DBF_DOUBLE,&(((aoRecord*)pRecord)->val)))
      {
         pRecord->udf = FALSE;
      }
      break;
   default:
      {
         return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                      "record type `%s' is not (yet) supported", pRecord->rdes->name);
      }
      break;
   }

   /* definition of the nolink option for callback functions */
   if (NULL == nolink)
   {
      nolink = (char*) calloc(10000,sizeof(char));
      if (NULL == nolink)
      {
         return errorDeactivateRecord(pRecord, __LINE__, "init_record","couldn't (re)allocate memory for 'nolink' string");
      }
      snprintf(nolink, 10000, "%s%s:%s %s",DIMNOLINK,__FILE__,__DATE__,__TIME__);
      nolink = (char*) realloc(nolink, (strlen(nolink)+1)*sizeof(char));
      if (NULL == nolink)
      {
         return errorDeactivateRecord(pRecord, __LINE__, "init_record","couldn't (re)allocate memory for 'nolink' string");
      }
   }

   /*
    * dynamic memory allocation,
    *  i.e. reallocate memory for control structures 
    */
   if(0 == recordIndex%MAX_DIMRecords)
   {
      recordVariablesArray = (struct recordVariables**) realloc( recordVariablesArray,
                             sizeof(struct recordVariables*) * (recordIndex + MAX_DIMRecords));
      if (NULL == recordVariablesArray)
      {
         return errorDeactivateRecord(pRecord, __LINE__, "init_record","couldn't (re)allocate memory for basic control structure");
      }
      assert(NULL != recordVariablesArray);
      /* clear new allocated part*/
      memset(&recordVariablesArray[recordIndex],0,MAX_DIMRecords);
   }

   /*
    * init and instatiation of a local record data set 
    * - one record set consists of the elements in the arrays at the same index
    * - internally the pointer of pRecord is stored in the recordVariablesArray struct 
    *   at the index nofDimRecords, so it can be found afterwards
    */

   struct recordVariables *thisRecord = NULL;
   thisRecord = createAndInitVariables(pRecord);

   /* check for failure */
   if( NULL == thisRecord )
   {
      /*
       * clear/delete existing remainders of struct recordVariables 
       */
      if (NULL != recordVariablesArray[recordIndex])
      {
         dumpStruct(recordVariablesArray[recordIndex],stderr);
         clearStructRecordVariables(recordVariablesArray[recordIndex]);
         recordVariablesArray[recordIndex] = NULL;
      }
      return errorDeactivateRecord(pRecord, __LINE__, "init_record","createAndInitVariables failed");
      backTrace(20);
   }
   assert(NULL != thisRecord);

   /*
    * DNS settings
    */

   /* node */
   if (false == setDnsAddress(thisRecord))
   {
      return errorDeactivateRecord(pRecord, __LINE__, "init_record","error calling setDnsAdress()");
   }
   /* port */
   if (false == setDnsPort(thisRecord))
   {
      return errorDeactivateRecord(pRecord, __LINE__, "init_record","error calling setDnsPort()");
   }

   /*
    * error routines 
    */
   if ( DIMSERVER == thisRecord->dimTierType )
   {
      /*init error and exit handler*/
      dis_add_error_handler( error_routine );
      dis_add_exit_handler ( exit_routine );
   }
   else /* DIMCLIENT */
   {
      dic_add_error_handler( error_routine );
   }

   /*
    * Setting up the DIM services, commands, servers and clients:
    * 
    * Differentiate the following 4 cases:
    * 	
    * 	A. DIM client:
    *                 1. connect to a provided service service via dic_info_service()
    *                 2.    call    a provided command service via dic_cmnd_service() (not done here but in process())
    *   B. DIM server:
    *                 1. 
    *                    a. provide a service service by registration via dis_add_service() and 
    *                    b. start serving it via start_serving()
    *                 2. 
    *                    a. provide a command service by registration via dis_add_cmnd() and 
    *                    b. start serving it via start_serving()
    * 
    */

   /* A.: DIM client */
   if( DIMCLIENT == thisRecord->dimTierType  )
   {
      /* A.1. connecting to a service service */
      if( true == thisRecord->dimServiceExists )
      {
         /* connect to service server */
         /* - asserts */
         assert(NULL != thisRecord->dimServiceName);

         thisRecord->dimServiceServiceID =
            dic_info_service(thisRecord->dimServiceName,
                             thisRecord->dimServiceScanType,
                             thisRecord->dimServiceScanInterval,
                             0, 0, dimCallback, recordIndex, &nolink, strlen(nolink)*sizeof(char) );
         /* check for failure */
         if ( 1 > thisRecord->dimServiceServiceID )
         {
            return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                         "could not connect to service server `%s' as client", thisRecord->dimServiceName);
         }
      }
   }

   /* B.: DIM server */
   if( DIMSERVER == thisRecord->dimTierType )
   {
      /*
       * B.1. providing a service service 
       */
      if( true == thisRecord->dimServiceExists )
      {
         /*init value for first publication*/
         /*better not cast here in the future*/
         if (0 > copyEpicsToDimData(thisRecord))
         {
            return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                         "copyEpicsToDimData() failed\n");
         }

         /*register DIM service service */
         /* - asserts */
         assert(0 != thisRecord->dataSize);
         assert(NULL != thisRecord->dimServiceName);
         assert(NULL != thisRecord->dimServiceFormat);
         assert(NULL != thisRecord->dimData);

         thisRecord->dimServiceServiceID =
            dis_add_service(thisRecord->dimServiceName,
                            thisRecord->dimServiceFormat,
                            thisRecord->dimData,
                            thisRecord->dataSize, NULL, 0 );
         /* successful ?? */
         if ( 0 == thisRecord->dimServiceServiceID )
         {
            return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                         "could not add service service `%s'",thisRecord->dimServiceName);
         }
      }

      /* B.2. providing a command service */
      if(true == thisRecord->dimCommandExists)
      {
         /*register DIM command service */
         thisRecord->dimCommandServiceID= dis_add_cmnd(thisRecord->dimCommandName,
                                          thisRecord->dimCommandFormat, dimCallback, recordIndex);
         /* check for failure */
         if ( 0 == thisRecord->dimCommandServiceID )
         {
            return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                         "could not add command service `%s'", thisRecord->dimServiceName);
         }
      }

      /* B.1.b and B.2.b: start serving services */

      /* prerequisites
       *   - create singleton: serverName */
      if (NULL == serverName)
      {
         /* determine DIM serverName
          * - out of the node name this process is running on
          * - its process id
          * - its port
          * - a prefix
          * -  : (2x) and @
          * to be unique as PREFIX:ProcessIDID@node:port
          */
         if ( false == makeServerName(pRecord))
         {
            SAFE_FREE(serverName);
            return errorDeactivateRecord(pRecord, __LINE__, "init_record",
                                         "makeServerName() failed");
         }
      }
      dis_start_serving( serverName );
   }
   dumpStruct(thisRecord, stdout);

   recordIndex++;
   return(0);
}

long copyDimToEpicsData(struct recordVariables *thisRecord)
{
   long status=-10;
   /* TODO: string to number conversion sscanf,atoi,atol,atof*/
   /* TODO: support structs */

   /* Since consistency of formats has been checked by checkConsistencyFormatTypes()
   * any available datatype discription can be chosen, 
   * TODO: regarding string transport mode */
   if ( false == thisRecord->dimServiceExists && false == thisRecord->dimCommandExists)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","copyDimToEpicsData",
              "no format defined, cannot copy values", ((dbCommon*)(thisRecord->recordAddress))->rdes->name);
      return -1;
   }

   char dimDataType = 0;
   char* dimDataTypeArray = NULL;

   if (true == thisRecord->dimServiceExists)
   {
      dimDataTypeArray = thisRecord->dimServiceDataTypes;
   }
   else /*(true == thisRecord->dimCommandExists)*/
   {
      dimDataTypeArray = thisRecord->dimCommandDataTypes;
   }

   switch(thisRecord->recordTypeIndex)
   {
   case DIMaiRecord:
      {
         struct aiRecord *pAiRecord = (aiRecord*) thisRecord->recordAddress;

         dimDataType = dimDataTypeArray[0];
         switch(dimDataType)
         {
         case 'I':
            pAiRecord->val = *((int *)thisRecord->dimData);
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'D':
            ((aiRecord*) thisRecord->recordAddress)->val = *((double *)thisRecord->dimData);
            ((aiRecord*) thisRecord->recordAddress)->rval = ((aiRecord*) thisRecord->recordAddress)->val;
            break;
         case 'S':
            pAiRecord->val = *((short *)thisRecord->dimData);
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'C':
            pAiRecord->val = *((char *)thisRecord->dimData);
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'L': /* TODO is this a valid assignment ?*/
            pAiRecord->val = *((long *)thisRecord->dimData);
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'X': /* TODO is this a valid assignment ?*/
            pAiRecord->val = *((longlong *)thisRecord->dimData);
            pAiRecord->rval = pAiRecord->val;
            break;
         default:
            {
               message(stderr,__FILE__,__LINE__,"ERROR","copyDimToEpicsData","DIM data type '%c` not (yet) supported",dimDataType);
               return -1;
            }
            break;
         }

         /* TODO: clarify status:
          * this line was originally in for ai record
          * 	status = dbGetLink(&(pAiRecord->inp),DBF_DOUBLE, &(pAiRecord->val),0,0);
          */
         status = 0;
      }
      break;
   case DIMaoRecord:
      {
         struct aoRecord *pAoRecord = (aoRecord*) thisRecord->recordAddress;
         /* TODO: string to number conversion sscanf,atoi,atol,atof*/
         /* TODO: support structs */

         dimDataType = dimDataTypeArray[0];
         switch(dimDataType)
         {
         case 'I':
            pAoRecord->val = *((int *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'D':
            pAoRecord->val = *((double *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'S':
            pAoRecord->val = *((short *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'C':
            pAoRecord->val = *((char *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'L': /* TODO is this a valid assignment ?*/
            pAoRecord->val = *((long *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'X': /* TODO is this a valid assignment ?*/
            pAoRecord->val = *((longlong *)thisRecord->dimData);
            pAoRecord->rval = pAoRecord->val;
            break;
         default:
            {
               message(stderr,__FILE__,__LINE__,"ERROR","copyDimToEpicsData", "DIM data type '%c` not (yet) supported",dimDataType);
               return -1;
            }
            break;
         }

         /* TODO: clarify status:
          * this line was originally in for ai record
          * 	status = dbGetLink(&(pAoRecord->out),DBF_DOUBLE, &(pAoRecord->val),0,0);
          */
         status = 0;
      }
      break;
   default:
      {
         message(stderr,__FILE__,__LINE__,"ERROR","copyDimToEpicsData", "record type `%s' is not (yet) supported", ((dbCommon*)(thisRecord->recordAddress))->rdes->name);
         /* TODO: What is the right way to react on this error? */
         return -1;
      }
   }
   if (0 > status)
   {
      message(stderr,__FILE__,__LINE__,"WARNING","copyDimToEpicsData",
              "status of dbGetLink %i, negative for record %s\n",
              status,((dbCommon*)(thisRecord->recordAddress))->name);
      return -1;

   }
   return status;
}

long copyEpicsToDimData(struct recordVariables *thisRecord)
{
   /* TODO: support structs */
   /* TODO: support arrays */

   /* Since consistency of formats has been checked by checkConsistencyFormatTypes()
    * any available datatype discription can be chosen, 
    * TODO: regarding string transport mode */
   if ( false == thisRecord->dimServiceExists && false == thisRecord->dimCommandExists)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","copyEpicsToDimData",
              "no format defined, cannot copy values", ((dbCommon*)(thisRecord->recordAddress))->rdes->name);
      return -1;
   }

   char dimDataType = 0;
   char* dimDataTypeArray = NULL;

   if (true == thisRecord->dimServiceExists)
   {
      dimDataTypeArray = thisRecord->dimServiceDataTypes;
   }
   else /*(true == thisRecord->dimCommandExists)*/
   {
      dimDataTypeArray = thisRecord->dimCommandDataTypes;
   }


   /* EPICS record type specific */
   switch(thisRecord->recordTypeIndex)
   {
   case DIMaiRecord:
      {
         struct aiRecord *pAiRecord = (aiRecord*) thisRecord->recordAddress;
         /* TODO: string to number conversion sscanf,atoi,atol,atof*/

         /* DIM type specific */
         dimDataType = dimDataTypeArray[0];
         switch(dimDataType)
         {
         case 'I':
            *((int *)thisRecord->dimData)      = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'D':
            *((double *)thisRecord->dimData)   = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'S':
            *((short *)thisRecord->dimData)    = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'C':
            *((char *)thisRecord->dimData)     = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'L':
            *((long *)thisRecord->dimData)     = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         case 'X':
            *((longlong *)thisRecord->dimData) = pAiRecord->val;
            pAiRecord->rval = pAiRecord->val;
            break;
         default:
            {
               message(stderr,__FILE__,__LINE__,"ERROR","copyEpicsToDimData", "DIM data type '%c` not (yet) supported",dimDataType);
               return -1;
            }
            break;
         }
      }
      break;
   case DIMaoRecord:
      {
         struct aoRecord *pAoRecord = (aoRecord*) thisRecord->recordAddress;

         /* TODO: string to number conversion sscanf,atoi,atol,atof*/

         /* DIM type specific */
         dimDataType = dimDataTypeArray[0];
         switch(dimDataType)
         {
         case 'I':
            *((int *)thisRecord->dimData)      = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'D':
            *((double *)thisRecord->dimData)   = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'S':
            *((short *)thisRecord->dimData)    = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'C':
            *((char *)thisRecord->dimData)      = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'L':
            *((long *)thisRecord->dimData)     = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         case 'X':
            *((longlong *)thisRecord->dimData) = pAoRecord->val;
            pAoRecord->rval = pAoRecord->val;
            break;
         default:
            {
               message(stderr,__FILE__,__LINE__,"ERROR","copyEpicsToDimData", "DIM data type '%c` not (yet) supported",dimDataType);
               return -1;
            }
            break;
         }
      }
      break;
   default:
      {
         message(stderr,__FILE__,__LINE__,"ERROR","copyEpicsToDimData", "record type `%s' is not (yet) supported", ((dbCommon*)(thisRecord->recordAddress))->rdes->name);
         return -1;
         break;
      }
   }
   return 0;
}

static long process(struct dbCommon* pRecord)
{
   bool doNotConvert=true; /*workaround for the conversion from rval to val und vice versa*/

   long status=-1;
   int ii=0;

   /* data flow:
   * cmnd from client:							DIM = value
   * svc on client (callback or scan active):	value = DIM
   * cmnd on server (callback):				    value = DIM
   * svc from server:							DIM = value
   */

   /*find index of record currently processed*/
   for(ii=0;ii<MAX_DIMRecords;ii++)
   {
      if( (*recordVariablesArray[ii]).recordAddress == (int*) pRecord)
         break;
   }

   recordVariables *processedRecord = recordVariablesArray[ii];

   if( 1 == processedRecord->calledFromCallback )
   {
      status = copyDimToEpicsData(processedRecord);
      if (0 > status)
      {
         message(stderr,__FILE__,__LINE__,"WARNING","process","copyDimToEpicsData failed\n");
         return 0;
      }

      /* reset toggle */
      processedRecord->calledFromCallback = 0;

      /* TODO: What is this??? */
      if( ((DIMCLIENT == processedRecord->dimTierType ) && status != 0) || (processedRecord->dimTierType == DIMSERVER) )
      {
         pRecord->udf = FALSE;
      }
      /* TODO: PSEUDOcodeif (record type ai && datatype not int than do not convert
       * i.e. return 2*/
      /*doNotConvert=true;*/

   }
   else /*"in process NOT calledFromCallback"*/
   {
      /* cmnd from client,
       * svc from server; 
       * dataflow EPICS -> DIM, 
       * DIM has to be informed about the change of the value*/
      if (0 > copyEpicsToDimData(processedRecord))
      {
         return errorDeactivateRecord(pRecord, __LINE__, "process","copyEpicsToDimData() failed\n", __LINE__);
      }

      /* TODO: check the logic and possible cases !!! */

      /*
       * Client calling a command: 
       */
      if( DIMCLIENT == processedRecord->dimTierType  &&  true == processedRecord->dimCommandExists )
      {
         /*TODO adopt and clean up*/

         int ack = dic_cmnd_service( processedRecord->dimCommandName,
                                     processedRecord->dimData,
                                     processedRecord->dataSize);
         if (ack != 1)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","process", "dic_cmnd_service for failed! ack for dic_cmnd_service: %i\n", ack);
         }

      }
      else  /*! (DIMCLIENT && dimCommandExists) */
      {
         if( DIMSERVER == processedRecord->dimTierType )
         {
            dis_update_service(processedRecord->dimServiceServiceID);
            /* TODO: Why is this set here to be FALSE */
            ((struct dbCommon*) pRecord)->udf = FALSE;
         }
         /* ( ! (DIMCLIENT && dimCommandExists) && ! DIMSERVER  ),
         * i.e. ! ( DIMCLIENT  
         * || (DIMSERVER && commandExist)  
         * || (DIMCLIENT && DIMSERVER)  
         * || (DIMCLIENT && dimCommandExists) ) 
         */
         else
         {
            /*alternative error ifelse concept appreciated*/
            if( processedRecord->dimTierType == DIMCLIENT )
            {
               message(stdout,NULL,0,"WARNING","process",
                       "Client without Command, only \"read\" permitted for record `%s'.\n",
                       ((dbCommon*)processedRecord->recordAddress)->name);
            }
            else
            {
               message(stdout,__FILE__,__LINE__,"ERROR","process",
                       "invalid TIER TYPE = %c in input field!\n", processedRecord->dimTierType);
            }
         }
      }
   }

   pRecord->pact = FALSE;

   /*
    * TODO: what is this ???
    */

   /*
       status=FALSE;
       if(status) {
   	recGblSetSevr(pRecord,LINK_ALARM,INVALID_ALARM);
   	return(status);
       }
       pRecord->udf = FALSE;
   */

   if (doNotConvert)
   {
      return 2;
   }
   else
   {
      return 0;
   }
}

void dimCallback( long *tag, int *data, int *size)
{
   /* TODO/NOTE:
    * I am aware, that in the documentation the second argument varies 
    * between *data and **data for service and command.
    * Nevertheless the use of **data lead to errors, when double dereferencing **data, I didn't understand.
    */
    
   struct dbCommon *pRecord;
   struct rset *prset;
   recordVariables *calledBackRecord = recordVariablesArray[(int)*tag];

   /*If the data passed by dim matches the content of nolink, then it is
    * the data of the *fillAddress parameter of dic_info_service, which is
    * only passed to callback if the service failed. By chance, the data
    * passed by dim could really be the same as nolink.*/

   /*In this implementation nolink is a string, while data is a double, therefore
    * minimizing the chances that the real content of data matches nolink.*/

   if ( DIMCLIENT == calledBackRecord->dimTierType )
   {
      if(! strncmp((char*)data,nolink,strlen(nolink)))
      {
         message(stdout,__FILE__,0,"INFO","dimCallback",
                 "Service service `%s' not available \n", calledBackRecord->dimServiceName);
         calledBackRecord->clientServiceCallbackOk = false;
         return;
      }

      /* back at work*/
      if (! calledBackRecord->clientServiceCallbackOk)
      {
         calledBackRecord->clientServiceCallbackOk = true;
         message(stdout,__FILE__,0,"INFO","dimCallback",
                 "Service service `%s' is back \n", calledBackRecord->dimServiceName);
      }
   }

   /* memcpy */
   /*
    * avoid that memory areas overlap 
    * i.e. A: beginning of data
    *      B: end of data (= A + size)
    *      C: beginning of dimData
    *      D: end of dimData ( = C + dataSize ) 
    * 
    *      3 cases invalid:
    *      - A == C
    *      - A > C  && C <= B
    *      - C > B  && D <= A 
    *      
    */

   if ((calledBackRecord->dimData > (void*) data
         && (calledBackRecord->dimData + calledBackRecord->dataSize) <= (void*) data)
         || ((void*)data > calledBackRecord->dimData
             && ((void*)data + *size) <= calledBackRecord->dimData)
         || (calledBackRecord->dimData == (void*) data))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","dimCallback()", "could't copy data, because memory overlap \n");
      return;
   }

   if (*size <= calledBackRecord->dataSize)
   {
      memcpy(calledBackRecord->dimData, (void*) data, *size);
   }
   else
   {
      memcpy(calledBackRecord->dimData, (void*) data, calledBackRecord->dataSize);
   }

   pRecord = (struct dbCommon*) (calledBackRecord->recordAddress);

   /* check for failure */
   if(NULL == pRecord)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","dimCallback", "pRecord in callback == NULL\n");
      return;
   }

   prset = (struct rset *) pRecord->rset;
   dbScanLock((struct dbCommon *)pRecord);
   /* set callback flag */
   calledBackRecord->calledFromCallback = 1;
   (*prset->process)(pRecord);
   dbScanUnlock((struct dbCommon *)pRecord);
   
   if ( DIMSERVER == calledBackRecord->dimTierType )
   {
   	  if ( true == calledBackRecord->dimServiceExists )
   	  {
   	  	dis_update_service(calledBackRecord->dimServiceServiceID);
   	  }
   }
}

void serverCmndCallback(int *tag, double *data, int *size)
{
   message(stdout,0,0,"INFO",0,"data pointer: %p, size: %i\n", data, *size);
   message(stdout,0,0,"INFO",0,"(double)data: %f\n", *data);
   message(stdout,0,0,"INFO",0,"(int)	data: %i\n", *(int*)data);
   message(stdout,0,0,"INFO",0,"(char*)	data: %s\n", (char*)data);
   message(stdout,0,0,"INFO",0,"(char)	data: %c\n", *(char*)data);

   message(stdout,0,0,"INFO",0,"CMND received from cmndClient and executed\n");
}

void error_routine (int severity, int error_code, char *messageString)
{
   char *errorString;
   errorString = (char*) malloc(1024*sizeof(char));
   switch (severity)
   {
   case DIM_INFO:
      sprintf(errorString,"DIM_INFO --- ");
      break;
   case DIM_WARNING:
      sprintf(errorString,"DIM_WARNING --- ");
      break;
   case DIM_ERROR:
      sprintf(errorString,"DIM_ERROR --- ");
      break;
   case DIM_FATAL:
      sprintf(errorString,"DIM_FATAL --- ");
      break;
   default:
      sprintf(errorString,"unknown severity: %i\n",severity);
      severity=DIM_FATAL;
      break;
   } 

   char value[2000];
   switch(error_code)
   {
   case DIMDNSUNDEF:
      strcat(errorString,"DIM_DNS_NODE undefined:");
      break;
   case DIMDNSREFUS:
      strcat(errorString,"DIM_DNS refuses connection:");
      break;
   case DIMDNSDUPLC:
      strcat(errorString,"Service already exists in DNS:");
      break;
   case DIMDNSEXIT:
      strcat(errorString,"DNS request server to EXIT:");
      break;
   case DIMDNSTMOUT:
      strcat(errorString,"Server failed sending watchdog:");
      break;
   case DIMDNSCNERR:
      strcat(errorString,"Connection to DNS failed:");
      break;
   case DIMDNSCNEST:
      strcat(errorString,"Connection to DNS established:");
      break;
   case DIMSVCDUPLC:
      strcat(errorString,"Service already exists in Server:");
      break;
   case DIMSVCFORMT:
      strcat(errorString,"Bad format string for service:");
      break;
   case DIMSVCINVAL:
      strcat(errorString,"Invalid Service ID:");
      break;
   case DIMTCPRDERR:
      strcat(errorString,"TCP/IP read error:");
      break;
   case DIMTCPWRRTY:
      strcat(errorString,"TCP/IP write error - Retrying:");
      break;
   case DIMTCPWRTMO:
      strcat(errorString,"TCP/IP write error - Disconnected:");
      break;
   case DIMTCPLNERR:
      strcat(errorString,"TCP/IP listen error:");
      break;
   case DIMTCPOPERR:
      strcat(errorString,"TCP/IP open server error :");
      break;
   case DIMTCPCNERR:
      strcat(errorString,"TCP/IP connection error:");
      break;
   case DIMTCPCNEST:
      strcat(errorString,"TCP/IP connection established:");
      break;
   default:
      strcat(errorString,"unknown error code: ");
      sprintf(value,"%i",severity);
      strncat(errorString,value,strlen(value));
      severity=DIM_FATAL;
      break;

   }

   if (1023 < strlen(messageString) + strlen(errorString))
   {
      errorString = (char*) realloc (errorString, sizeof(char)*(strlen(messageString)+1024));
   }

   strncat(errorString,messageString, strlen(messageString)+1);

   char * error_services = dic_get_error_services ();
   if (NULL != error_services)
   {
      char msg[]="\nerroneous services: ";
      if (1023 + strlen(messageString) < strlen(error_services) + strlen(errorString)+strlen(msg)+1)
      {
         errorString = (char*) realloc (errorString,
                                        sizeof(char)*(strlen(error_services)+strlen(errorString)+strlen(msg)+2));
      }
      strncat(errorString, msg, strlen(msg)+1);
      strncat(errorString, error_services, strlen(error_services)+1);
   }
   strncat(errorString,"\n",2);
   message(stderr, NULL, 0, NULL, NULL, "%s" ,errorString);

   if (DIM_FATAL == severity)
   {
      switch(error_code)
      {
      case DIMDNSEXIT:
         free(errorString);
         return exit_routine(&error_code);
         break;
      default:
         /* do nothing */
         break;
      }
   }
}

void exit_routine (int *code)
{
   /* "Destructor" */
   if (0 != *code)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","exit_routine",
              "Fatal DIM error or an exit command was received. Error code: %i.\n", *code);
   }
   int i=0;
   for (i=0; i < recordIndex; i++)
   {
      if (NULL != recordVariablesArray[i])
      {
         clearStructRecordVariables(recordVariablesArray[i]);
      }
   }
   /* epicsExit(0);*/
}

void dumpStruct ( struct recordVariables *rec, FILE *stream )
{
   int i=0;
   message(stream, NULL, 0, "DUMP", NULL,"Record ---\n");
   if (rec->recordAddress)
   {
      message(stream, NULL, 0, "DUMP", NULL, "%s`%s'\n","--- Record Name: ",((struct dbCommon*)rec->recordAddress)->name);
   }
   message(stream, NULL, 0, "DUMP", NULL,
           "\tPOINTER       : record address ................................................ (recordAddress) : %p\n"
           ,rec->recordAddress);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : record index .......................................................... (index) : %i\n"
           ,rec->index);
   message(stream, NULL, 0, "DUMP", NULL,
           rec->recordType       ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: record type ...................................................... (recordType) : "
           ,rec->recordType);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : record type index ........................................... (recordTypeIndex) : %i\n"
           ,rec->recordTypeIndex);

   message(stream, NULL, 0, "DUMP", NULL,
           "DATA ---\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tPOINTER       : data Pointer ........................................................ (dimData) : %p\n"
           ,rec->dimData);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tUNSIGNED INT  : data size .......................................................... (dataSize) : %i\n"
           ,rec->dataSize);

   message(stream, NULL, 0, "DUMP", NULL,"FLAGS --\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : calledFromCallback .................................................... (index) : %i\n"
           ,rec->calledFromCallback);

   message(stream, NULL, 0, "DUMP", NULL,"DNS --\n");
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimDnsAddress    ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: DIM DNS Address ............................................... (dimDnsAddress) : "
           ,rec->dimDnsAddress);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : DIM DNS PORT ..................................................... (dimDnsPort) : %i\n"
           ,rec->dimDnsPort);

   message(stream, NULL, 0, "DUMP", NULL,"TIER ---\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tCHAR          : TIER type ....................................................... (dimTierType) : '%c'\n"
           ,rec->dimTierType);
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimServicesNamePrefix       ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: SERVER NAME PREFIX .................................... (dimServicesNamePrefix) : "
           ,rec->dimServicesNamePrefix);

   message(stream, NULL, 0, "DUMP", NULL,"Command ---\n");
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimCommandName   ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: COMMAND NAME ................................................. (dimCommandName) : "
           ,rec->dimCommandName);
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimCommandFormat ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: COMMAND FORMAT ............................................. (dimCommandFormat) : "
           ,rec->dimCommandFormat);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : no. of Command Formats ............................ (dimNumberOfCommandFormats) : %i\n"
           ,rec->dimNumberOfCommandFormats);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tCHAR ARRAY    : command Data types ...................................... (dimCommandDataTypes) : ");
   if (rec->dimCommandDataTypes)
   {
      for (i=0;i<rec->dimNumberOfCommandFormats;i++)
      {
         message(stream, NULL, 0, NULL, NULL,"%s'%c'\t",i?"|":"",rec->dimCommandDataTypes[i]);
      }
   }
   else
   {
      message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimCommandDataTypes);
   }
   message(stream, NULL, 0,  NULL, NULL,"\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER ARRAY : no. of command Data types .............. (dimNumberOfCommandDataTypesPerFormat) : ");
   if (rec->dimNumberOfCommandDataTypesPerFormat)
   {
      for (i=0;i<rec->dimNumberOfCommandFormats;i++)
      {
         if (0 == rec->dimNumberOfCommandDataTypesPerFormat[i])
         {

            message(stream, NULL, 0, NULL, NULL,"%s 0 (=any)\t",i?"|":"");
         }
         else
         {
            message(stream, NULL, 0, NULL, NULL,"%s %i\t",i?"|":"",rec->dimNumberOfCommandDataTypesPerFormat[i]);
         }
      }
   }
   else
   {
      message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimNumberOfCommandDataTypesPerFormat);
   }
   message(stream, NULL, 0,  NULL, NULL,"\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : command exists ............................................. (dimCommandExists) : %i (=%s)\n"
           ,rec->dimCommandExists,rec->dimCommandExists?"true":"false");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : Command Service ID ...................................... (dimCommandServiceID) : %i\n"
           ,rec->dimCommandServiceID);

   message(stream, NULL, 0, "DUMP", NULL,
           "Service ---\n");
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimServiceName   ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: SERVICE NAME ........................................................ (svcName) : "
           ,rec->dimServiceName);
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimServiceFormat ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: SERVICE FORMAT ............................................. (dimServiceFormat) : "
           ,rec->dimServiceFormat);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : no. of Service Formats ............................ (dimNumberOfServiceFormats) : %i\n"
           ,rec->dimNumberOfServiceFormats);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tCHAR ARRAY    : service Data types ...................................... (dimServiceDataTypes) : ");
   if (rec->dimServiceDataTypes)
   {
      for (i=0;i<rec->dimNumberOfServiceFormats;i++)
      {
         message(stream, NULL, 0, NULL, NULL,"%s'%c'\t",i?"|":"",rec->dimServiceDataTypes[i]);
      }
   }
   else
   {
      message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimServiceDataTypes);
   }
   message(stream, NULL, 0, NULL, NULL,"\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER ARRAY : no. of service Data types .............. (dimNumberOfServiceDataTypesPerFormat) : ");
   if (rec->dimNumberOfServiceDataTypesPerFormat)
   {
      for (i=0;i<rec->dimNumberOfServiceFormats;i++)
      {
         if (0 == rec->dimNumberOfServiceDataTypesPerFormat[i])
         {
            message(stream, NULL, 0, NULL, NULL,"%s 0 (=any)\t",i?"|":"");
         }
         else
         {
            message(stream, NULL, 0, NULL, NULL,"%s %i\t",i?"|":"",rec->dimNumberOfServiceDataTypesPerFormat[i]);
         }
         message(stream, NULL, 0, NULL, NULL,"| %i\t",rec->dimNumberOfServiceDataTypesPerFormat[i]);
      }
   }
   else
   {
      message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimNumberOfServiceDataTypesPerFormat);
   }
   message(stream, NULL, 0, NULL, NULL,"\n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : service exists ............................................. (dimServiceExists) : %i (=%s)\n"
           ,rec->dimServiceExists,rec->dimServiceExists?"true":"false");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : Service Service ID ...................................... (dimServiceServiceID) : %i\n"
           ,rec->dimServiceServiceID);
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : Client Service Service Scan Type ......................... (dimServiceScanType) : %i"
           ,rec->dimServiceScanType);
   switch (rec->dimServiceScanType)
   {
   case MONITORED:
      message(stream, NULL, 0, NULL, NULL, " ( = MONITORED )\n");
      break;
   case TIMED:
      message(stream, NULL, 0, NULL, NULL, " ( = TIMED )\n");
      break;
   default:
      message(stream, NULL, 0, NULL, NULL, " ( = UNKNOWN )\n");
      break;
   }
   message(stream, NULL, 0, "DUMP", NULL,
           "\tINTEGER       : Client Service Service Scan Interval ................. (dimServiceScanInterval) : %i\n"
           ,rec->dimServiceScanInterval);

   message(stream, NULL, 0, "DUMP", NULL,"Xtensions ---\n");
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimPutDefaultSuffix ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: DEFAULT PUT SUFFIX ...................................... (dimPutDefaultSuffix) : "
           ,rec->dimPutDefaultSuffix);
   message(stream, NULL, 0, "DUMP", NULL,
           rec->dimGetDefaultSuffix ? "%s`%s'\n":"%s%p\n",
           "\tSTRING POINTER: DEFAULT GET SUFFIX ...................................... (dimGetDefaultSuffix) : "
           ,rec->dimGetDefaultSuffix);

   message(stream, NULL, 0, "DUMP", NULL,"             --- automatic creation \n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : auto create both - Service and Command ........................ (dimCreateBoth) : %i (=%s)\n"
           ,rec->dimCreateBoth,rec->dimCreateBoth?"true":"false");

   message(stream, NULL, 0, "DUMP", NULL,"             --- string transport \n");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : transport via strings for commands ................... (stringTransportCommand) : %i (=%s)\n"
           ,rec->stringTransportCommand,rec->stringTransportCommand?"true":"false");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : transport via strings for services ................... (stringTransportService) : %i (=%s)\n"
           ,rec->stringTransportService,rec->stringTransportService?"true":"false");
   message(stream, NULL, 0, "DUMP", NULL,
           "\tFLAG          : transport via strings for both .......................... (stringTransportBoth) : %i (=%s)\n"
           ,rec->stringTransportBoth,rec->stringTransportBoth?"true":"false");

   if ( false == rec->stringTransportCommand )
   {
      message(stream, NULL, 0, "DUMP", NULL,"\t.............. ommitting string transport outputs for commands\n");
   }
   else
   {
      message(stream, NULL, 0, "DUMP", NULL,
              rec->dimStringTransportCommandFormat ? "%s`%s'\n":"%s%p\n",
              "\tSTRING POINTER: STRING TRANSPORT COMMAND FORMAT ..............(dimStringTransportCommandFormat) : "
              ,rec->dimStringTransportCommandFormat);

      message(stream, NULL, 0, "DUMP", NULL,
              "\tINTEGER       : # String Transport Command Formats . (dimStringTransportNumberOfCommandFormats) : %i\n"
              ,rec->dimStringTransportNumberOfCommandFormats);
      message(stream, NULL, 0, "DUMP", NULL,
              "\tCHAR ARRAY    : String Transport command Data types ...... (dimStringTransportCommandDataTypes) : ");
      if (rec->dimStringTransportCommandDataTypes)
      {
         for (i=0;i<rec->dimStringTransportNumberOfCommandFormats;i++)
         {
            message(stream, NULL, 0, NULL, NULL,"%s'%c'\t",i?"|":"",rec->dimStringTransportCommandDataTypes[i]);
         }
      }
      else
      {
         message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimStringTransportCommandDataTypes);
      }
      message(stream, NULL, 0, "DUMP", NULL,"\n");
      message(stream, NULL, 0, "DUMP", NULL,
              "\tINTEGER ARRAY : StrTrans CMND Data types  (dimStringTransportNumberOfCommandDataTypesPerFormat) : ");
      if (rec->dimStringTransportNumberOfCommandDataTypesPerFormat)
      {
         for (i=0;i<rec->dimStringTransportNumberOfCommandFormats;i++)
         {
            if (0 == rec->dimStringTransportNumberOfCommandDataTypesPerFormat[i])
            {
               message(stream, NULL, 0, NULL, NULL,"%s 0 (=any)\t",i?"|":"");
            }
            else
            {
               message(stream, NULL, 0, NULL, NULL,"%s %i\t",i?"|":"",rec->dimStringTransportNumberOfCommandDataTypesPerFormat[i]);
            }
         }
      }
      else
      {
         message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimStringTransportNumberOfCommandDataTypesPerFormat);
      }
      message(stream, NULL, 0, NULL, NULL,"\n");
   }
   if ( false == rec->stringTransportService )
   {
      message(stream, NULL, 0, "DUMP", NULL,"\t.............. ommitting string transport outputs for commands\n");
   }
   else
   {
      message(stream, NULL, 0, "DUMP", NULL,
              "\tINTEGER       : # String Transport Service Formats . (dimStringTransportNumberOfServiceFormats) : %i\n"
              ,rec->dimStringTransportNumberOfServiceFormats);
      message(stream, NULL, 0, "DUMP", NULL,
              "\tCHAR ARRAY    : String Transport command Data types ...... (dimStringTransportServiceDataTypes) : ");
      if (rec->dimStringTransportServiceDataTypes)
      {
         for (i=0;i<rec->dimStringTransportNumberOfServiceFormats;i++)
         {
            message(stream, NULL, 0, NULL, NULL,"%s'%c'\t",i?"|":"",rec->dimStringTransportServiceDataTypes[i]);
         }
      }
      else
      {
         message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimStringTransportServiceDataTypes);
      }
      message(stream, NULL, 0, NULL, NULL,"\n");
      message(stream, NULL, 0, "DUMP", NULL,
              "\tINTEGER ARRAY : StrTrans CMND Data types  (dimStringTransportNumberOfServiceDataTypesPerFormat) : ");
      if (rec->dimStringTransportNumberOfServiceDataTypesPerFormat)
      {
         for (i=0;i<rec->dimStringTransportNumberOfServiceFormats;i++)
         {
            if (0 == rec->dimStringTransportNumberOfServiceDataTypesPerFormat[i])
            {
               message(stream, NULL, 0, NULL, NULL,"%s 0 (=any)\t",i?"|":"");
            }
            else
            {
               message(stream, NULL, 0, NULL, NULL,"%s %i\t",i?"|":"",rec->dimStringTransportNumberOfServiceDataTypesPerFormat[i]);
            }
         }
      }
      else
      {
         message(stream, NULL, 0, NULL, NULL,"%p"       ,rec->dimStringTransportNumberOfServiceDataTypesPerFormat);
      }
      message(stream, NULL, 0, NULL, NULL,"\n");
   }

   message(stream, NULL, 0, "DUMP", NULL,"-------------------------------------------------------------------------------------------------------------------------\n");
   fflush(stream);
}

void clearStructRecordVariables( struct recordVariables *thisRecord )
{
   thisRecord->dimData = NULL;
   thisRecord->dataSize = 0;
   thisRecord->recordAddress = NULL;
   thisRecord->index = 0;
   thisRecord->dimCommandServiceID = -1;
   thisRecord->dimServiceServiceID = -1;
   thisRecord->dimTierType = '0';
   thisRecord->calledFromCallback = -1;
   if (  NULL != thisRecord->dimServiceName)
   {
      safeStringFree(&thisRecord->dimServiceName, strlen(thisRecord->dimServiceName));
   }
   if (  NULL != thisRecord->dimServiceFormat)
   {
      safeStringFree(&thisRecord->dimServiceFormat, strlen(thisRecord->dimServiceFormat));
   }
   if (  NULL != thisRecord->dimCommandName)
   {
      safeStringFree(&thisRecord->dimCommandName, strlen(thisRecord->dimCommandName));
   }
   if (  NULL != thisRecord->dimCommandFormat)
   {
      safeStringFree(&thisRecord->dimCommandFormat, strlen(thisRecord->dimCommandFormat));
   }
   if (  NULL != thisRecord->dimServicesNamePrefix)
   {
      safeStringFree(&thisRecord->dimServicesNamePrefix, strlen(thisRecord->dimServicesNamePrefix));
   }
   thisRecord->dimServiceExists = false;
   thisRecord->dimCommandExists = false;
   if ( NULL != thisRecord->recordType  )
   {
      safeStringFree(&thisRecord->recordType  ,strlen(thisRecord->recordType));
   }
   if ( NULL != thisRecord->dimDnsAddress)
   {
      safeStringFree(&thisRecord->dimDnsAddress,strlen(thisRecord->dimDnsAddress));
   }
   thisRecord->dimDnsPort = -1;
   thisRecord->recordTypeIndex = -1;

   if ( NULL != thisRecord->dimServiceDataTypes)
   {
      safeStringFree(&thisRecord->dimServiceDataTypes, thisRecord->dimNumberOfServiceFormats);
   }
   if ( NULL != thisRecord->dimCommandDataTypes)
   {
      safeStringFree(&thisRecord->dimCommandDataTypes, thisRecord->dimNumberOfServiceFormats);
   }

   thisRecord->dimNumberOfServiceFormats = 0;
   thisRecord->dimNumberOfCommandFormats = 0;
   if ( NULL != thisRecord->dimNumberOfServiceDataTypesPerFormat)
   {
      SAFE_FREE(thisRecord->dimNumberOfServiceDataTypesPerFormat);
   }
   if ( NULL != thisRecord->dimNumberOfCommandDataTypesPerFormat)
   {
      SAFE_FREE(thisRecord->dimNumberOfCommandDataTypesPerFormat);
   }

   if ( NULL != thisRecord->dimGetDefaultSuffix)
   {
      safeStringFree(&thisRecord->dimGetDefaultSuffix, strlen(thisRecord->dimGetDefaultSuffix));
   }

   if ( NULL != thisRecord->dimPutDefaultSuffix)
   {
      safeStringFree(&thisRecord->dimPutDefaultSuffix, strlen(thisRecord->dimPutDefaultSuffix));
   }

   thisRecord->dimServiceScanType = -1;
   thisRecord->dimServiceScanInterval = -1;
   thisRecord->clientServiceCallbackOk = true;
   thisRecord->serverCommandCallbackOk = true;

   thisRecord->stringTransportCommand = false;
   thisRecord->stringTransportService = false;
   thisRecord->stringTransportBoth = false;

   if (  NULL != thisRecord->dimStringTransportServiceFormat)
   {
      safeStringFree(&thisRecord->dimStringTransportServiceFormat, strlen(thisRecord->dimStringTransportServiceFormat));
   }
   if (  NULL != thisRecord->dimStringTransportCommandFormat)
   {
      safeStringFree(&thisRecord->dimStringTransportCommandFormat, strlen(thisRecord->dimStringTransportCommandFormat));
   }
   thisRecord->dimStringTransportNumberOfServiceFormats = 0;
   thisRecord->dimStringTransportNumberOfCommandFormats = 0 ;
   if ( NULL != thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat)
   {
      SAFE_FREE(thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat);
   }
   if ( NULL != thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat)
   {
      SAFE_FREE(thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat);
   }

   if ( NULL != thisRecord->dimStringTransportServiceDataTypes)
   {
      safeStringFree(&thisRecord->dimStringTransportServiceDataTypes, thisRecord->dimStringTransportNumberOfServiceFormats);
   }
   if ( NULL != thisRecord->dimCommandDataTypes)
   {
      safeStringFree(&thisRecord->dimCommandDataTypes, thisRecord->dimNumberOfServiceFormats);
   }
   thisRecord->dimCreateBoth = false;
}

recordVariables* createAndInitVariables( struct dbCommon *pRecord )
{
   int     i  = 0;
   int     found_elements=0;
   char  **keyArray     =NULL;
   char  **argumentArray=NULL;
   char  **formatArray  =NULL;

   char *inputOutputField =  NULL;
   char *inputOutput;
   int   sizeOfInputOutput = 0;
   DBLINK inputOutputLink;
   struct recordVariables *thisRecord = NULL;

   if (!pRecord)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "pointer to record (function's arg 1) is NULL\n");
      return NULL;
   }

   /* allocate memory for 1 recordVariables*/
   thisRecord = (recordVariables*) calloc(1, sizeof(recordVariables));
   /* calloc failed  */
   if( !thisRecord)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "couldn't allocate memory\n");
      return NULL;
   }
   /* copy pointer to global variables Array */
   recordVariablesArray[recordIndex] = thisRecord;
   /* set default values recordVariables */
   clearStructRecordVariables(thisRecord);

   /*
    * set index and record Address 
    */
   thisRecord->index = recordIndex;
   thisRecord->recordAddress = pRecord;

   /*
    * determine record type (dbCommon->dbRecordType->name (p->rdes->name)
    */
   thisRecord->recordTypeIndex = determineRecordType(pRecord);
   if (0 <= thisRecord->recordTypeIndex)
   {
      thisRecord->recordType = malloc(sizeof(char)*(1+strlen(DIMInterfaceSupportedRecordNames[thisRecord->recordTypeIndex])));
      if (NULL == thisRecord->recordType)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "couldn't allocate memory for record description name string ... returning NULL\n");
         return NULL;
      }
      strncpy(thisRecord->recordType,
              DIMInterfaceSupportedRecordNames[thisRecord->recordTypeIndex],
              strlen(DIMInterfaceSupportedRecordNames[thisRecord->recordTypeIndex])+1);
   }
   /*
    * evaluate inp/out field of record 
    */
   switch(thisRecord->recordTypeIndex)
   {
   case DIMaiRecord:
      inputOutputLink = ((struct aiRecord*)pRecord)->inp;
      break;
   case DIMaoRecord:
      inputOutputLink = ((struct aoRecord*)pRecord)->out;
      break;
   default:
      message(stderr,__FILE__,__LINE__,"ERROR","init_record", "record type `%s' is not (yet) supported", pRecord->rdes->name);
      return NULL;
      break;
   }

   /*
    * TODO: check if inputOutputLink is constantStr or string
    *       - not yet the right check because it is 11 (CA_LINK)
    */
   /*
      switch(inputOutputLink.type)
      {
      case CONSTANT:
         inputOutputField = inputOutputLink.value.constantStr;
         break;
      case INST_IO:
         inputOutputField = inputOutputLink.value.instio.string;
         break;
      default:
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "INP/OUT linktype %i not supported (only CONSTANT,(INST_IO))(see link.h) ... returning NULL\n", inputOutputLink.type);
                  return NULL;
         break;
      }
   */

   inputOutputField = inputOutputLink.value.constantStr;

   /* check for failure */
   if( NULL == inputOutputField )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "pointer to constStr of INP/OUT field NULL ... returning NULL\n");
      return NULL;
   }

   sizeOfInputOutput = strlen(&(inputOutputField)[0]);
   /* check for failure */
   if (1 > sizeOfInputOutput)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "empty inputOutput string ... cannot continue\n");
      return NULL;
   }

   inputOutput = (char*) malloc(sizeof(char) * (sizeOfInputOutput+1));
   /* check for failure */
   if( NULL == inputOutput)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "couldn't allocate memory ... returning NULL\n");
      return NULL;
   }

   strcpy( &inputOutput[0], &inputOutputField[0] );

   /* disassemble inputOutput string */
   found_elements = disassembleString(inputOutput, &keyArray, &argumentArray, &formatArray,
                                      DIM_PRIMARY_TOKEN, DIM_SECONDARY_TOKEN, DIM_THIRD_TOKEN, DIM_SPACE_TOKEN, DIM_TOKEN_PROTECTION );
   safeStringFree(&inputOutput,sizeOfInputOutput+1);

   /* analyze */
   for (i = 0; i < found_elements; i++)
   {
      switch (toupper(keyArray[i][0]))
      {
      case DIMTIERTYPE:
         /* check for failure: argument empty */
         if (0 == strlen(argumentArray[i]))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "argument of key `%s%c' is empty ... returning NULL\n",DIM_PRIMARY_TOKEN,DIMTIERTYPE);
            return NULL;
         }
         switch(toupper(argumentArray[i][0]))
         {
         case DIMSERVER:
            thisRecord->dimTierType = DIMSERVER;
            break;
         case DIMCLIENT:
            thisRecord->dimTierType = DIMCLIENT;
            break;
         default:
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "`%c' is not a valid letter of the argument for %s%c ... returning NULL\n",argumentArray[i][0],DIM_PRIMARY_TOKEN,DIMTIERTYPE);
            return NULL;
            break;
         }
         break;
      case DIMSERVERSNAMEPREFIX:
         if (! setRecordVariablesString(&(thisRecord->dimServicesNamePrefix), argumentArray[i], "dimServicesNamePrefix", DIMSERVERSNAMEPREFIX) )
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
            return NULL;
         }
         break;
      case DIMSERVICE:
         if (!keyArray[i][1])
         {
            if (! setRecordVariablesString(&(thisRecord->dimServiceName), argumentArray[i], "dimServiceName", DIMSERVICE) )
            {
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
               return NULL;
            }
            thisRecord->dimServiceExists = true;
         }
         else
         {
            switch(tolower(keyArray[i][1]))
            {
            case DIMFORMAT:
               {
                  /* analysis of format */
                  if (! setRecordVariablesString(&(thisRecord->dimServiceFormat), argumentArray[i], "dimServiceFormat", '\"'))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                     return NULL;
                  }

                  if (!disassembleFormat(thisRecord->dimServiceFormat,
                                         &thisRecord->dimNumberOfServiceFormats,
                                         &thisRecord->dimServiceDataTypes,
                                         &thisRecord->dimNumberOfServiceDataTypesPerFormat))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "disassembleFormat() failed");
                     return NULL;
                  }
                  thisRecord->dimServiceExists = true;
               }
               break;
            case DIMSERVICESCANINTERVAL:
               {
                  if (0 <= thisRecord->dimServiceScanInterval)
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "clients service scan interval (timeout) already set to %i, what to choose? ... returning NULL\n",
                             thisRecord->dimServiceScanInterval);
                     return NULL;
                  }
                  /* only integers are allowed, check for decimal points or commas */
                  if (false == checkValidUnsignedInt(argumentArray[i]))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "clients service scan interval (timeout) is not set to a valid unsigned integer but something different: `%s' ... returning NULL\n",
                             argumentArray[i]);
                     return NULL;
                  }
                  else
                  {
                     thisRecord->dimServiceScanInterval = atoi(argumentArray[i]);
                  }
               }
               break;
            case DIMSERVICESCANTYPE:
               if (false == setCombinedClientServiceScanTypeAndInterval(thisRecord, argumentArray[i]))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setCombinedClientServiceScanTypeAndInterval failed");
                  return NULL;
               }
               break;
            default:
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "`%s' is not a valid Key ... returning NULL\n",keyArray[i]);
               return NULL;
            } /*// end switch(tolower(keyArray[i][1])) */
         }
         break;
      case DIMCOMMAND:
         if (!keyArray[i][1])
         {
            if (! setRecordVariablesString(&(thisRecord->dimCommandName), argumentArray[i], "dimCommandName", DIMCOMMAND) )
            {
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
               return NULL;
            }
            thisRecord->dimCommandExists = true;
         }
         else
         {
            if (DIMFORMAT == tolower(keyArray[i][1]))
            {
               /* analysis of format */
               if (! setRecordVariablesString(&(thisRecord->dimCommandFormat), argumentArray[i], "dimCommandFormat", '\"'))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                  return NULL;
               }

               if (! disassembleFormat(thisRecord->dimCommandFormat,
                                       &thisRecord->dimNumberOfCommandFormats,
                                       &thisRecord->dimCommandDataTypes,
                                       &thisRecord->dimNumberOfCommandDataTypesPerFormat))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "disassembleFormat() failed");
                  return NULL;
               }
               thisRecord->dimCommandExists = true;
            }
            else
            {
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "`%s' is not a valid Key ... returning NULL\n",keyArray[i]);
               return NULL;
            }
         }
         break;
      case DIMDNSADDRESS:
         /* check for failure: string too long*/
         if ( MAXIMUMDIMDNSADDRESSLENGTH < strlen(argumentArray[i]) )
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "`%s' is too long, max: %i ... returning NULL\n",
                    keyArray[i], MAXIMUMDIMDNSADDRESSLENGTH);
            return NULL;
         }
         if ( false == setCombinedDnsAddressAndPort(thisRecord, argumentArray[i]))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setCombinedDnsAddressAndPort() failed");
            return NULL;
         }
         break;
      case DIMDNSPORT:
         if (false == checkValidUnsignedInt(argumentArray[i]))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "dns port is not set to a valid unsigned integer: `%s' ... returning NULL\n",
                    argumentArray[i]);
            return NULL;
         }
         else
         {
            thisRecord->dimDnsPort = atoi(argumentArray[i]);
         }
         break;
      case DIMEXTENSIONS:
         if (keyArray[i][1])
         {
            switch(tolower(keyArray[i][1]))
            {
            case DIMEXTENSIONSGETSUFFIX:
               if (! setRecordVariablesString(&(thisRecord->dimGetDefaultSuffix),
                                              argumentArray[i], "dimGetDefaultSuffix",
                                              DIMEXTENSIONSGETSUFFIX) )
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                  return NULL;
               }
               break;
            case DIMEXTENSIONSPUTSUFFIX:
               if (! setRecordVariablesString(&(thisRecord->dimPutDefaultSuffix),
                                              argumentArray[i], "dimPutDefaultSuffix",
                                              DIMEXTENSIONSPUTSUFFIX) )
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                  return NULL;

               }
               break;
            case DIMEXTENSIONSCREATE:
               if (keyArray[i][2])
               {
                  switch(tolower(keyArray[i][2]))
                  {
                  case DIMEXTENSIONSCREATEBOTH:
                     thisRecord->dimCreateBoth = true;
                     break;
                  default:
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "%c%c`%s' is not a valid extension string transport Key ... returning NULL\n",
                             DIMEXTENSIONS,DIMEXTENSIONSCREATE,&keyArray[i][2]);
                     return NULL;
                     break;
                  }
               }
               break;
            case DIMEXTENSIONSTRINGTRANSPORT:
               if (keyArray[i][2])
               {
                  switch(tolower(keyArray[i][2]))
                  {
                  case DIMEXTENSIONSTRINGTRANSPORTSERVICE:
                     if (! setRecordVariablesString(&(thisRecord->dimStringTransportServiceFormat),
                                                    argumentArray[i], "dimStringTransportServiceFormat",
                                                    DIMEXTENSIONS))
                     {
                        message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                        return NULL;

                     }
                     if (! disassembleFormat(thisRecord->dimStringTransportServiceFormat,
                                             &thisRecord->dimStringTransportNumberOfServiceFormats,
                                             &thisRecord->dimStringTransportServiceDataTypes,
                                             &thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat))
                     {
                        message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "disassembleFormat() failed");
                        return NULL;
                     }
                     thisRecord->stringTransportService = true;
                     break;
                  case DIMEXTENSIONSTRINGTRANSPORTCOMMAND:
                     if (! setRecordVariablesString(&(thisRecord->dimStringTransportCommandFormat),
                                                    argumentArray[i], "dimStringTransportCommandFormat",
                                                    DIMEXTENSIONS))
                     {
                        message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setRecordVariablesString() failed");
                        return NULL;

                     }
                     if (! disassembleFormat(thisRecord->dimStringTransportCommandFormat,
                                             &thisRecord->dimStringTransportNumberOfCommandFormats,
                                             &thisRecord->dimStringTransportCommandDataTypes,
                                             &thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat))
                     {
                        message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "disassembleFormat() failed");
                        return NULL;
                     }
                     thisRecord->stringTransportCommand = true;
                     break;
                  case DIMEXTENSIONSTRINGTRANSPORTBOTH:
                     thisRecord->stringTransportBoth = true;
                     break;
                  default:
                     message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "%c%c`%s' is not a valid extension string transport Key ... returning NULL\n",
                             DIMEXTENSIONS,DIMEXTENSIONSTRINGTRANSPORT,&keyArray[i][2]);
                     return NULL;
                     break;
                  }
               }
               else
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "%c%c is an incomplete key ... returning NULL\n",
                          DIMEXTENSIONS,DIMEXTENSIONSTRINGTRANSPORT);
                  return NULL;
               }
               break;
            default:
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "%c`%s' is not a valid extension Key ... returning NULL\n",
                       DIMEXTENSIONS,&keyArray[i][1]);
               return NULL;
               break;
            }
         }
         else
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "%c is an incomplete key ... returning NULL\n",
                    DIMEXTENSIONS);
            return NULL;
         }
         break;
      default:
         message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables()", "`%s' is not a valid Key ... returning NULL\n",keyArray[i]);
         return NULL;
         break;
      }
      /*
      // end:  switch (toupper(keyArray[i][0]))
       */
   }
   /*
   // end: for (i = 0; i < found_elements; i++)
    */
   /* ************************************************************************************************************** */

   safeFreeOfDisassemblyStringArrays(found_elements,keyArray,argumentArray,formatArray);

   /* ************************************************************************************************************** */
   /* consistency checks, default settings, dependencies */

   /* TODO: (maybe later in this code) checks for consistency of format='C[:N]' and stringTransport
    * resulting in stringTransport... = true, 
    * also Xsb option to be evaluated */
   /*
    * TODO:check for string transport (record specific)??
    * */


   /* settings:
    * 
    * CLIENT for service service 
    * - if the dim service type scan type is not set (=0) 
    *   (instead of MONITORED (=2), TIMED (=4), 
    *   (ONCE_ONLY (=1) does not make sense))
    * 
    *   => MONITORED is set as default value
    */
   if (DIMCLIENT == thisRecord->dimTierType)
   {
      if (0 >= thisRecord->dimServiceScanType)
      {
         thisRecord->dimServiceScanType = MONITORED;
         if (0 > thisRecord->dimServiceScanInterval)
         {
            thisRecord->dimServiceScanInterval=0;
         }
      }
   }

   /* check:
    * 
    * CLIENT for service service 
    * - if the dim service type scan type is set to TIMED 
    *   the scan interval has to be different from 0
    */
   if (DIMCLIENT == thisRecord->dimTierType)
   {
      if (TIMED == thisRecord->dimServiceScanType)
      {
         if (0 >= thisRecord->dimServiceScanInterval )
         {
            message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "mandatory clients service scan interval (timeout) is not set for mode MONITORED... returning NULL\n");
            return NULL;
         }
      }
   }

   /* settings:
    * 
    * SERVER: autogenerate missing services 
    * - in case only one service is defined: switchable with Xoption create both
    *   else automatically create both services 
    * - names 
    * 	- if only one name is specified, append (DEFAULT)SUFFIXES
    * 	- if both are missing use recordname, append (DEFAULT)SUFFIXES
    * - the same also for formats
    */

   if (DIMSERVER == thisRecord->dimTierType)
   {
      if (true == thisRecord->dimCreateBoth || ((NULL == thisRecord->dimCommandName) && (NULL == thisRecord->dimServiceName)))
      {
         if ((!thisRecord->dimCommandName) || (!thisRecord->dimServiceName))
         {
            if (!supplementServersNamesAndFormats(thisRecord))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "supplementServersNamesAndFormats() failed");
               return NULL;
            }
         }
      }
   }

   /* check:
    *  
    * CLIENT: 
    * - if at least one Service name is set 
    */

   if (DIMCLIENT == thisRecord->dimTierType)
   {
      if((!thisRecord->dimCommandName) && (!thisRecord->dimServiceName))
      {
         message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "You have to at least specify  either a Command, via `%s%c%s', or a Service, via `%s%c%s' ... returning NULL\n",
                 DIM_PRIMARY_TOKEN, DIMCOMMAND, DIM_SECONDARY_TOKEN, DIM_PRIMARY_TOKEN, DIMSERVICE, DIM_SECONDARY_TOKEN);
         return NULL;
      }
   }

   /* settings:
    * 
    * SERVER/CLIENT: Services prefix
    * - if set serversPrefix,
    *   then add it in front of the service names 
    *   in the form prefix/name, if name exists
    *   (where the seperator '/' can be chosen by DIMSERVICESPREFIXSEPARATOR)
    */

   if ( false == addServersPrefix(thisRecord) )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "addServersPrefix() failed ... returning NULL\n");
      return NULL;
   }

   /* settings:
    * 
    * CLIENT: retrieve format
    *  - if format not set, 
    *    retrieve it via  
    *    - id = dic_info/cmnd_service, 
    *    - get_dic_format(id)
    *    - release(id)
    * TODO: possible also for Commands?
    */
   if ( false == retrieveAndSetClientServicesFormat(thisRecord) )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "retrieveAndSetClientServicesFormat() failed ... returning NULL\n");
      return NULL;
   }

   /* settings:
    * 
    * SERVER: 
    * - if server and no format is specified, set default datatypes,
    *   record specificly defined via DIMrecordDEFAULTDATATYPE 
    *  (record= AI,AO,STRING, ..)
   */
   if (false == setServersDefaultDataFormat(thisRecord))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "setServersDefaultDataFormat() failed ... returning NULL\n");
      return NULL;
   }


   /* check:
    * 
    * CLIENT/SERVER: identical format types
    *  - if both, service and command, are set
    *  	- check if format types of service and command are identical
    * TODO:  - check also in case of string transport mode
    */
   if (false == checkConsistencyFormatTypes(thisRecord))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "checkConsistencyFormatTypes() failed\n");
      return NULL;
   }

   /*
    * determine needed datasize 
    */
   thisRecord->dataSize = determineDataSize(thisRecord);
   if ( 0 == thisRecord->dataSize)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "determineDataSize() failed\n");
      return NULL;
   }

   /*
    *  type specific allocation 
    * 
    * (those relies upon previous check: check if service and command if formats are identical). s.a.
    */

   thisRecord->dimData = malloc( thisRecord->dataSize );
   if ( NULL == thisRecord->dimData)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "couldn't allocate memory for dimData ... returning NULL\n");
      return NULL;
   }

   /*
    * record specific consistency checks:
    */

   if (false == checkConsistencyRecordSpecific(thisRecord))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","createAndInitVariables", "checkConsistencyRecordSpecific() failed\n");
      return NULL;
   }

   /*
    * set callback flag 
    */
   thisRecord->calledFromCallback = 0;

   /* *************************************************************************************************************** */

   return thisRecord;
}

bool retrieveAndSetClientServicesFormat(struct recordVariables *thisRecord)
{
   if (DIMCLIENT == thisRecord->dimTierType)
   {
      if ( thisRecord->dimServiceExists )
      {
         if (NULL == thisRecord->dimServiceFormat)
         {
            if (false == getServiceFormat(thisRecord->dimServiceFormat, thisRecord))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "getServiceFormat() failed\n");
               return false;
            }

            if (false == setRecordVariablesString(&(thisRecord->dimServiceFormat), thisRecord->dimServiceFormat, "dimServiceFormat", 0))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "setRecordVariablesString failed\n");
               return false;
            }
            if (false == disassembleFormat(thisRecord->dimServiceFormat,
                                           &thisRecord->dimNumberOfServiceFormats,
                                           &thisRecord->dimServiceDataTypes,
                                           &thisRecord->dimNumberOfServiceDataTypesPerFormat))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "disassembleFormat() failed");
               return false;
            }
         }
      }
      if ( thisRecord->dimCommandExists )
      {
         if (NULL == thisRecord->dimCommandFormat)
         {
            if (false == getCommandFormat(thisRecord->dimCommandFormat, thisRecord))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "getCommandFormat() failed ... returning NULL\n");
               return false;
            }

            if (false == setRecordVariablesString(&(thisRecord->dimCommandFormat),
                                                  thisRecord->dimCommandFormat, "dimCommandFormat", 0))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "setRecordVariablesString failed\n");
               return false;
            }
            if (false == disassembleFormat(thisRecord->dimCommandFormat,
                                           &thisRecord->dimNumberOfCommandFormats,
                                           &thisRecord->dimCommandDataTypes,
                                           &thisRecord->dimNumberOfCommandDataTypesPerFormat))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","retrieveAndSetClientServicesFormat", "disassembleFormat() failed");
               return false;
            }
         }
      }
   }
   return true;
}

bool getServiceFormat(char *serviceFormat, struct recordVariables *thisRecord)
{
   assert(NULL != thisRecord->dimServiceName);
   int dummy[100];
   /*   int dummy2 = -1000 ;*/
   int id = dic_info_service(thisRecord->dimServiceName, ONCE_ONLY, 0, 0, 0, getDimFormatCallback, thisRecord->index, dummy, sizeof(int)*100);/*dim_call_back, -1, dummy, sizeof(dummy));*/
   char *format = dic_get_format(id);
   dic_release_service(id);
   printf("format = %s\n",format);

   if ( NULL == format )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","getServiceFormat", "empty format string ... returning NULL\n");
      return false;
   }
   serviceFormat = (char*) malloc(sizeof(char) * (strlen(&format[0])+1));
   if (NULL == serviceFormat)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","getServiceFormat", "couldn't allocate memory for format  ... returning NULL\n");
      return false;
   }
   strncpy(serviceFormat, format, strlen(&format[0]));
   return true;
}

void getDimFormatCallback(long *tag,  void *data,  int *size)
{
   /*recordVariablesArray[tag]->*/
   char *format = dic_get_format(0);
   fprintf(stderr,"LINE %i: ",__LINE__);
   printf("************ format = %s\n",format);

}

bool getCommandFormat(char *commandFormat, struct recordVariables *thisRecord)
{
   /* TODO: does this work ??? */
   int dummy = -1000;
   int id = dic_info_service(thisRecord->dimCommandName, MONITORED, 0, 0, 0, dimCallback, -1, dummy, sizeof(dummy));
   char *format = dic_get_format(id);
   dic_release_service(id);

   printf("format = %s\n",format);

   if ( NULL == format )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","getCommandFormat", "empty format string ... returning NULL\n");
      return false;
   }
   commandFormat = (char*) malloc(sizeof(char) * (strlen(&format[0])+1));
   if (NULL ==  commandFormat)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","getCommandFormat", "couldn't allocate memory for format  ... returning NULL\n");
      return false;
   }
   strncpy(commandFormat, format, strlen(&format[0]));
   return true;
}

bool addServersPrefix(struct recordVariables *thisRecord)
{
   /*
    * Servers prefix
    * if set, then add it in front of the service names in the form prefix/name, if name exists
    * (where the seperator '/' can be chosen by DIMSERVICESPREFIXSEPARATOR)
    */
   if (NULL != thisRecord->dimServicesNamePrefix)
   {
      /* command */
      if (NULL != thisRecord->dimCommandName)
      {
         char* tmp = NULL;
         thisRecord->dimCommandName = (char*) realloc(thisRecord->dimCommandName,
                                      sizeof(char) * ( 1
                                                       + strlen(thisRecord->dimCommandName)
                                                       + strlen(DIMSERVICESPREFIXSEPARATOR)
                                                       + strlen(thisRecord->dimServicesNamePrefix))
                                                     );
         if (NULL == thisRecord->dimCommandName)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","addServersPrefix", "couldn't allocate memory for attaching prefix to service names  ... returning NULL\n");
            return false;
         }
         tmp = (char*) calloc(( 1
                                + strlen(thisRecord->dimCommandName)
                                + strlen(DIMSERVICESPREFIXSEPARATOR)
                                + strlen(thisRecord->dimServicesNamePrefix)),
                              sizeof(char)
                             );
         if (NULL == tmp)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","addServersPrefix", "couldn't allocate memory for attaching prefix to service names  ... returning NULL\n");
            return false;
         }
         strncpy(tmp, thisRecord->dimServicesNamePrefix, strlen(thisRecord->dimServicesNamePrefix));
         strncat(tmp, DIMSERVICESPREFIXSEPARATOR, strlen(DIMSERVICESPREFIXSEPARATOR));
         strncat(tmp, thisRecord->dimCommandName, strlen(thisRecord->dimCommandName));

         strncpy(thisRecord->dimCommandName,
                 tmp,
                 1
                 + strlen(thisRecord->dimCommandName)
                 + strlen(DIMSERVICESPREFIXSEPARATOR)
                 + strlen(thisRecord->dimServicesNamePrefix)
                );
         SAFE_FREE(tmp);
      }
      /* service */
      if (NULL != thisRecord->dimServiceName)
      {
         char* tmp = NULL;
         thisRecord->dimServiceName = (char*) realloc(thisRecord->dimServiceName,
                                      sizeof(char) * ( 1
                                                       + strlen(thisRecord->dimServiceName)
                                                       + strlen(DIMSERVICESPREFIXSEPARATOR)
                                                       + strlen(thisRecord->dimServicesNamePrefix))
                                                     );
         if (NULL == thisRecord->dimServiceName)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","addServersPrefix", "couldn't allocate memory for attaching prefix to service names  ... returning NULL\n");
            return false;
         }
         tmp = (char*) calloc(( 1
                                + strlen(thisRecord->dimServiceName)
                                + strlen(DIMSERVICESPREFIXSEPARATOR)
                                + strlen(thisRecord->dimServicesNamePrefix))
                              ,sizeof(char)
                             );
         if (NULL == tmp)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","addServersPrefix", "couldn't allocate memory for attaching prefix to service names  ... returning NULL\n");
            return false;
         }
         strncpy(tmp, thisRecord->dimServicesNamePrefix, strlen(thisRecord->dimServicesNamePrefix));
         strncat(tmp, DIMSERVICESPREFIXSEPARATOR, strlen(DIMSERVICESPREFIXSEPARATOR));
         strncat(tmp, thisRecord->dimServiceName, strlen(thisRecord->dimServiceName));
         strncpy(thisRecord->dimServiceName,
                 tmp,
                 1
                 + strlen(thisRecord->dimServiceName)
                 + strlen(DIMSERVICESPREFIXSEPARATOR)
                 + strlen(thisRecord->dimServicesNamePrefix)
                );
         SAFE_FREE(tmp);
      }
   }
   return true;
}

bool setServersDefaultDataFormat(struct recordVariables *thisRecord)
{
   /*
    * if service server and no format is specified, 
    * record specific 
    * ai/ao:  DIMAIAODEFAULTDATATYPE 
    * string/stringout: DIMSTRINGDEFAULTDATATYPE
    * TODO: expand 
   */
   if (DIMSERVER == thisRecord->dimTierType)
   {
      char datatype[10000] = {'\0'};
      switch(thisRecord->recordTypeIndex)
      {
      case DIMaiRecord:
      case DIMaoRecord:
         strncpy(datatype,DIMAIAODEFAULTDATATYPE,10000);
         break;
      case DIMstringinRecord:
      case DIMstringoutRecord:
         strncpy(datatype,DIMSTRINGDEFAULTDATATYPE,10000);
         break;
      default:
         {
            message(stderr,__FILE__,__LINE__,"ERROR","setServersDefaultDataFormat", "record type `%s' not supported ... returning NULL\n",
                    DIMInterfaceSupportedRecordNames[thisRecord->recordTypeIndex]);
            return false;
         }
         break;
      }

      if ( thisRecord->dimCommandExists )
      {
         if (NULL == thisRecord->dimCommandFormat)
         {
            if (! setRecordVariablesString(&(thisRecord->dimCommandFormat), datatype, "dimCommandFormat", '\"'))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setServersDefaultDataFormat", "setRecordVariablesString failed\n");
               return false;
            }
            if (! disassembleFormat(thisRecord->dimCommandFormat,
                                    &thisRecord->dimNumberOfCommandFormats,
                                    &thisRecord->dimCommandDataTypes,
                                    &thisRecord->dimNumberOfCommandDataTypesPerFormat))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setServersDefaultDataFormat", "disassembleFormat() failed");
               return false;
            }
         }
      }
      if ( thisRecord->dimServiceExists )
      {
         if (NULL == thisRecord->dimServiceFormat)
         {
            if (! setRecordVariablesString(&(thisRecord->dimServiceFormat), datatype, "dimServiceFormat", '\"'))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setServersDefaultDataFormat", "setRecordVariablesString failed\n");
               return false;
            }
            if (! disassembleFormat(thisRecord->dimServiceFormat,
                                    &thisRecord->dimNumberOfServiceFormats,
                                    &thisRecord->dimServiceDataTypes,
                                    &thisRecord->dimNumberOfServiceDataTypesPerFormat))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setServersDefaultDataFormat", "disassembleFormat() failed");
               return false;
            }
         }
      }
   }
   return true;
}

bool checkConsistencyFormatTypes(struct recordVariables *thisRecord)
{
   /*
   * check if format types of service and command  are identical
   * 	- in case both service and command are set
   *    - also check in case of string transport mode
   */

   if (true == thisRecord->dimCommandExists && true == thisRecord->dimServiceExists)
   {
      /*string transport*/
      if (true == thisRecord->stringTransportCommand || true == thisRecord->stringTransportService)
      {
         /* both */
         if (thisRecord->stringTransportCommand && thisRecord->stringTransportService)
         {
            if ( 0 != strncmp(thisRecord->dimStringTransportCommandFormat,
                              thisRecord->dimStringTransportServiceFormat,
                              strlen(thisRecord->dimServiceFormat)))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","checkConsistencyFormatTypes",
                       "data formats of command `%s': `%s' (string transport) and service `%s': `%s' (string transport) are not the same\n",
                       thisRecord->dimCommandName,
                       thisRecord->dimStringTransportCommandFormat,
                       thisRecord->dimServiceName,
                       thisRecord->dimStringTransportServiceFormat);
               return false;
            }
         }
         /* command only */
         else if (true == thisRecord->stringTransportCommand && false == thisRecord->stringTransportService)
         {
            if ( 0 != strncmp(thisRecord->dimStringTransportCommandFormat,
                              thisRecord->dimServiceFormat,
                              strlen(thisRecord->dimServiceFormat)))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","checkConsistencyFormatTypes",
                       "data formats of command `%s': `%s' (string transport) and service `%s': `%s' are not the same\n",
                       thisRecord->dimCommandName,
                       thisRecord->dimStringTransportCommandFormat,
                       thisRecord->dimServiceName,
                       thisRecord->dimServiceFormat);
               return false;
            }
         }

         /* service only */
         else /*(false == thisRecord->stringTransportCommand && true == thisRecord->stringTransportService)*/
         {
            if ( 0 != strncmp(thisRecord->dimCommandFormat,
                              thisRecord->dimStringTransportServiceFormat,
                              strlen(thisRecord->dimServiceFormat)))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","checkConsistencyFormatTypes",
                       "data formats of command `%s': `%s' and service `%s': `%s' (string transport) are not the same\n",
                       thisRecord->dimCommandName,
                       thisRecord->dimCommandFormat,
                       thisRecord->dimServiceName,
                       thisRecord->dimStringTransportServiceFormat);
               return false;
            }
         }
      }
      else /*(false == thisRecord->stringTransportCommand || false == thisRecord->stringTransportService)*/
      {
         if ( 0 != strncmp(thisRecord->dimCommandFormat,
                           thisRecord->dimServiceFormat,
                           strlen(thisRecord->dimServiceFormat)))
         {
            message(stderr,__FILE__,__LINE__,"ERROR", "checkConsistencyFormatTypes",
                    "data formats of command `%s': `%s' and service `%s': `%s' are not the same\n",
                    thisRecord->dimCommandName,
                    thisRecord->dimCommandFormat,
                    thisRecord->dimServiceName,
                    thisRecord->dimServiceFormat);
            return false;
         }
      }
   }
   return true;
}

bool checkConsistencyRecordSpecific(struct recordVariables *thisRecord)
{
   /* 1: (not) supporting arrays, structs
   * 1.a. special case string transport
   * 2: TODO: datatype checks
   */

   switch(thisRecord->recordTypeIndex)
   {
   case DIMaiRecord:
   case DIMaoRecord:
      {
         /* command */
         if (true == thisRecord->dimCommandExists)
         {
            if (1 != thisRecord->dimNumberOfCommandFormats)
            {
               message(stderr,__FILE__,__LINE__,"ERROR",
                       "checkConsistencyRecordSpecific", "structures not supported by `%sRecord', like '%s`",
                       thisRecord->recordType, thisRecord->dimCommandFormat);
               return false;
            }
            assert(1 == thisRecord->dimNumberOfCommandFormats);
            if ( false == thisRecord->stringTransportCommand )
            {
               if (1 != thisRecord->dimNumberOfCommandDataTypesPerFormat[0] )
               {
                  message(stderr,__FILE__,__LINE__,"ERROR",
                          "checkConsistencyRecordSpecific", "arrays not supported, by `%sRecord', like '%s`",
                          thisRecord->recordType, thisRecord->dimCommandFormat);
                  return false;
               }
            }
         }
         /* service */
         if (true == thisRecord->dimServiceExists)
         {
            if (1 != thisRecord->dimNumberOfServiceFormats)
            {
               message(stderr,__FILE__,__LINE__,"ERROR",
                       "checkConsistencyRecordSpecific", "structures not supported by `%sRecord', like '%s`\n",
                       thisRecord->recordType, thisRecord->dimServiceFormat);
               return false;
            }
            assert(1 == thisRecord->dimNumberOfServiceFormats);
            if ( false == thisRecord->stringTransportService )
            {
               if (1 != thisRecord->dimNumberOfServiceDataTypesPerFormat[0])
               {
                  message(stderr,__FILE__,__LINE__,"ERROR",
                          "checkConsistencyRecordSpecific", "arrays not supported, by `%sRecord', like '%s`\n",
                          thisRecord->recordType, thisRecord->dimServiceFormat);
                  return false;
               }
            }
         }
         break;
      default:
         {
            message(stderr,__FILE__,__LINE__,"ERROR",
                    "checkConsistencyRecordSpecific", "record type `%s' is not (yet) supported\n",
                    ((dbCommon*)thisRecord->recordAddress)->rdes->name);
            return false;
            break;
         }
      }
   }
   return true;
}

unsigned int determineDataSize(struct recordVariables *thisRecord)
{
   unsigned int dataSize = 0;

   if (false == checkConsistencyFormatTypes(thisRecord))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","determineDataSize", "checkConsistencyFormatType failed");
      return 0;
   }


   /* string transport active */
   if (true == thisRecord->stringTransportCommand || true == thisRecord->stringTransportService)
   {
      if (true == thisRecord->stringTransportCommand) /* also includes case both, where consistency has been checked previously */
      {
         dataSize = calculateDataSize(thisRecord->dimStringTransportNumberOfCommandFormats,
                                      thisRecord->dimStringTransportCommandDataTypes,
                                      thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat);

      }
      else if(true == thisRecord->stringTransportService)
      {
         dataSize = calculateDataSize(thisRecord->dimStringTransportNumberOfServiceFormats,
                                      thisRecord->dimStringTransportServiceDataTypes,
                                      thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat);
      }
   }
   else /* normal transport */
   {
      if (true == thisRecord->dimCommandExists) /* also includes case both, where consistency has been checked previously */
      {
         dataSize = calculateDataSize(thisRecord->dimNumberOfCommandFormats,
                                      thisRecord->dimCommandDataTypes,
                                      thisRecord->dimNumberOfCommandDataTypesPerFormat);

      }
      else if (true == thisRecord->dimServiceExists)
      {
         dataSize = calculateDataSize(thisRecord->dimNumberOfServiceFormats,
                                      thisRecord->dimServiceDataTypes,
                                      thisRecord->dimNumberOfServiceDataTypesPerFormat);
      }
   }

   if (0 == dataSize)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","determineDataSize", "dataSize = %i, invalid value\n",
              dataSize);
      return 0;
   }
   else
   {
      return dataSize;
   }
}

unsigned int calculateDataSize(unsigned int numberOfFormats,
                               char dataTypes[],
                               unsigned int numberOfDataTypesPerFormat[])
{
   unsigned int dataSize = 0;
   int format=0;
   int number=0;
   char dimDataType = 0;

   if (0 == numberOfFormats)
   {

      message(stderr,__FILE__,__LINE__,"ERROR",
              "calculateDataSize", "number of formats == 0!\n");
      return 0;
   }
   if (NULL == dataTypes)
   {
      message(stderr,__FILE__,__LINE__,"ERROR",
              "calculateDataSize", "array of dataTypes empty!\n");
      return 0;
   }
   if (NULL == numberOfDataTypesPerFormat)
   {
      message(stderr,__FILE__,__LINE__,"ERROR",
              "calculateDataSize", "array of number of dataTypes per format empty!\n");
      return 0;
   }
   for (format =0; format < numberOfFormats; format++)
   {
      dimDataType = dataTypes[format];
      for (number =0; number < numberOfDataTypesPerFormat[number]; number++)
      {
         switch(dimDataType)
         {
         case 'I':
            dataSize += sizeof(int);
            break;
         case 'D':
            dataSize += sizeof(double);
            break;
         case 'S':
            dataSize += sizeof(short);
            break;
         case 'C':
            dataSize += sizeof(char);
            break;
         case 'L':
            dataSize += sizeof(long);
            break;
         case 'X':
            dataSize += sizeof(longlong);
            break;
         default:
            {
               message(stderr,__FILE__,__LINE__,"ERROR",
                       "calculateDataSize", "DIM data type '%c` not (yet) supported",dimDataType);
               return 0;
            }
            break;
         }
      }
   }
   return dataSize;
}

char* setRecordVariablesString(char **element, char argument[], char discription[], char key)
{
   if (argument && strlen(argument))
   {
      *element = malloc(sizeof(char)*(1+strlen(argument)));
      if( !*element)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","setRecordVariablesString", "couldn't allocate memory for %s string\n",discription);
         return NULL;
      }
      strncpy (&((*element)[0]), argument, 1+strlen(argument) );
   }
   else
   {
      if (0 != key)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","setRecordVariablesString","argument of key `%s%c' is empty\n",DIM_PRIMARY_TOKEN,key);
      }
      else
      {
         message(stderr,__FILE__,__LINE__,"ERROR","setRecordVariablesString","argument is empty\n");
      }
      return NULL;
   }
   return *element;
}

bool checkValidType(char *type)
{
   bool isValid=false;
   int i = 0;
   int value = 0;
   if (!type)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","checkValidType", "argument NULL ... returning false");
      isValid=false;
   }
   else
   {
      value=toupper(type[0]);
      isValid=false;
      for (i=0; i<DIMDataTypeLastDummyEntry; i++)
      {
         if ( DIMInterfaceSupportedDIMDataTypesNames[i] == value)
         {
            isValid=true;
            break;
         }
      }
      if (!isValid)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","checkValidType", "invalid format type `%c' ... returning false\n",type[0]);
      }
   }
   return isValid;
}

bool disassembleFormat(char formatString[],
                       unsigned int* dimNumberOfCommandFormats,
                       char** dimCommandDataTypes,
                       unsigned int** dimNumberOfCommandDataTypesPerFormat )
{
   int     dimFormatIndex = 0, dimTypeIndex=0;
   int     foundFormatArguments=0;
   int     value=0;
   char  **typeArray    =NULL;
   int    *numberArray  =NULL;

   *dimNumberOfCommandFormats = disassembleDIMFormatString(formatString,&typeArray,&numberArray, DIM_FOURTH_TOKEN, DIM_FIFTH_TOKEN, DIM_TOKEN_PROTECTION);

   if(*dimNumberOfCommandFormats >0)
   {
      /*
       * TODO: extend to structs
       * status: do not allow more than one type
       *         and skip if not 
       */

      if(*dimNumberOfCommandFormats >1)
      {
         message(stderr,__FILE__,__LINE__,"ERROR","disassembleFormat", "cannot support structures (up to NOW),%s`%s'%s\n",
                 "i.e. only one element type is allowed, but not",formatString,"  ... returning false");
         return false;
      }

      *dimCommandDataTypes = malloc(sizeof(char)*((*dimNumberOfCommandFormats)+1));
      *dimNumberOfCommandDataTypesPerFormat = malloc(sizeof(char)*((*dimNumberOfCommandFormats)+1));

      for (dimFormatIndex = 0; dimFormatIndex < *dimNumberOfCommandFormats; dimFormatIndex++)
      {
         /* check for valid data type */
         if (!checkValidType(typeArray[dimFormatIndex]))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","disassembleFormat", "checkValidType() failed");
            return false;
         }
         value=toupper(typeArray[dimFormatIndex][0]);

         for (dimTypeIndex=0; dimTypeIndex<DIMDataTypeLastDummyEntry; dimTypeIndex++)
         {
            if ( DIMInterfaceSupportedDIMDataTypesNames[dimTypeIndex] == value)
            {
               (*dimCommandDataTypes)[dimFormatIndex] = DIMInterfaceSupportedDIMDataTypesNames[dimTypeIndex];
               (*dimNumberOfCommandDataTypesPerFormat)[dimFormatIndex] = numberArray[dimFormatIndex];
               break;
            }
         }

         safeStringFree(&typeArray[dimFormatIndex],strlen(typeArray[dimFormatIndex]));
      }
   }
   safePArrayFree((void**)typeArray,foundFormatArguments);
   SAFE_FREE(numberArray);
   /* fprintf(stdout,"\n");
    * fflush(stdout);*/
   return true;
}

epicsEnum16 determineRecordType(struct dbCommon* pRecord)
{
   /*
    * determine record type (dbCommon->dbRecordType->name (p->rdes->name)
    * - return value
    * 		-1 : if no match was found
    *     enum of enum DIMInterfaceSupportedRecords
    */

   char* name=NULL;
   int i =0;
   if (pRecord)
   {
      if (pRecord->rdes)
      {
         if (pRecord->rdes->name)
         {
            name = malloc(sizeof(char)*(1+strlen(pRecord->rdes->name)));
            if (name)
            {
               strncpy(name, pRecord->rdes->name, strlen(pRecord->rdes->name)+1);
            }
            else
            {
               message(stderr,__FILE__,__LINE__,"ERROR","determineRecordType", "couldn't allocate memory for record description name string ... returning -1\n");
               return -1;
            }
         }
      }

      /*
       * compare name with supported records
       */

      for (i=0; i<DIMLastDummyEntry; i++)
      {
         if (0 == strncmp(&name[0],
                          &DIMInterfaceSupportedRecordNames[i][0],
                          strlen(&DIMInterfaceSupportedRecordNames[i][0])))
         {
            SAFE_FREE(name);
            return i;
         }
      }
   }
   SAFE_FREE(name);
   return -1;
}

bool supplementServersNamesAndFormats(struct recordVariables *thisRecord)
{
   /* this feature is switchable with Xoption both
    * in case of a SERVER: 
    * - if only one name is specified, 
    *     - create the other out of it by appending (DEFAULT)SUFFIXES
    *     - if format is set for only one, 
    *        - copy format to the other
    *     - string transport formats are also copied
    * - if both are missing 
    *     - use recordname and append (DEFAULT)SUFFIXES
    *     - leave format open will be handled either by settings or by defaults
    */
   char **address = NULL;
   char  *name    = NULL;
   char  *suffix  = NULL;
   char  *title   = NULL;

   if (thisRecord->dimTierType == DIMSERVER)
   {
      if (thisRecord->dimCreateBoth || ((NULL == thisRecord->dimCommandName) && (NULL == thisRecord->dimServiceName)))
      {
         if ((!thisRecord->dimCommandName) || (!thisRecord->dimServiceName))
         {
            /*
            * neither command name nor service name are given
            */
            if ((!thisRecord->dimCommandName) && (!thisRecord->dimServiceName))
            {
               /*create new service name: name+suffix*/
               address = &thisRecord->dimServiceName;
               name    = ((struct dbCommon*)thisRecord->recordAddress)->name;
               suffix  = thisRecord->dimGetDefaultSuffix?thisRecord->dimGetDefaultSuffix:DIMDEFAULTGETSUFFIX;
               title   = "dimServiceName";
               assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

               if (false == recreateServersSupplementNames(address, name, suffix, title))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                  return false;
               }
               thisRecord->dimServiceExists = true;

               /*create new command name: name+suffix*/
               address = &thisRecord->dimCommandName;
               name    = ((struct dbCommon*)thisRecord->recordAddress)->name;
               suffix  = thisRecord->dimPutDefaultSuffix?thisRecord->dimPutDefaultSuffix:DIMDEFAULTPUTSUFFIX;
               title   = "dimCommandName";
               assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

               if (false == recreateServersSupplementNames(address, name, suffix, title))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                  return false;
               }
               thisRecord->dimCommandExists = true;
            }
            else
            {
               /*
                * service name given but no command name
                */
               if ((!thisRecord->dimCommandName) && (thisRecord->dimServiceName))
               {
                  /*create new command name: name+suffix*/
                  address = &thisRecord->dimCommandName;
                  name    = thisRecord->dimServiceName;
                  suffix  = thisRecord->dimPutDefaultSuffix?thisRecord->dimPutDefaultSuffix:DIMDEFAULTPUTSUFFIX;
                  title   = "dimCommandName";
                  assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

                  if (false == recreateServersSupplementNames(address, name, suffix, title))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                     return false;
                  }
                  thisRecord->dimCommandExists = true;

                  /*extend service name: name+suffix*/

                  address = &thisRecord->dimServiceName;
                  name    = thisRecord->dimServiceName;
                  suffix  = thisRecord->dimGetDefaultSuffix?thisRecord->dimGetDefaultSuffix:DIMDEFAULTGETSUFFIX;
                  title   = "dimServiceName";
                  assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

                  if (false == recreateServersSupplementNames(address, name, suffix, title))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                     return false;
                  }

                  /* format */
                  if (false == supplementServersFormat(thisRecord))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "supplementServersFormat() failed\n");
                     return false;
                  }

                  /* format string transport*/
                  if (false == supplementStringTransportServersFormat(thisRecord))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "supplementStringTransportServersFormat() failed\n");
                     return false;
                  }
               }
               /*
               * command name given but no service name
               */
               else if ((thisRecord->dimCommandName) && (!thisRecord->dimServiceName))
               {
                  /*create new service name: name+suffix*/
                  address = &thisRecord->dimServiceName;
                  name    = thisRecord->dimCommandName;
                  suffix  = thisRecord->dimGetDefaultSuffix?thisRecord->dimGetDefaultSuffix:DIMDEFAULTGETSUFFIX;
                  title   = "dimServiceName";
                  assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

                  if (false == recreateServersSupplementNames(address, name, suffix, title))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                     return false;
                  }
                  thisRecord->dimServiceExists = true;

                  /*extend command name: name+suffix*/
                  address = &thisRecord->dimCommandName;
                  name    = thisRecord->dimCommandName;
                  suffix  = thisRecord->dimPutDefaultSuffix?thisRecord->dimPutDefaultSuffix:DIMDEFAULTPUTSUFFIX;
                  title   = "dimCommandName";
                  assert( NULL != address && NULL != name && NULL != suffix && NULL != title);

                  if (false == recreateServersSupplementNames(address, name, suffix, title))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "recreateServersSupplementNames() failed\n");
                     return false;
                  }

                  /* format */
                  if (false == supplementServersFormat(thisRecord))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "supplementServersFormat() failed\n");
                     return false;
                  }
                  /* format string transport*/
                  if (false == supplementStringTransportServersFormat(thisRecord))
                  {
                     message(stderr,__FILE__,__LINE__,"ERROR","supplementServersNamesAndFormats", "supplementStringTransportServersFormat() failed\n");
                     return false;
                  }
               }
            }
         }
      }
   }
   return true;
}

bool recreateServersSupplementNames(char **element, char name[], char suffix[], char title[])
{
   if (NULL == element || NULL == name || NULL == suffix || NULL == title)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","recreateServersSupplementNames", "argument(s) NULL\n");
      return false;
   }

   /*create new name*/
   unsigned int length = 1 + strlen(name) + strlen(suffix);
   char *newName       = (char*) calloc(length, sizeof(char));
   /* alloc failed ? */
   if (NULL == newName)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","recreateServersSupplementNames", "couldn't (re)allocate memory\n");
      return false;
   }
   /* fill */
   snprintf(newName, length, "%s%s", name, suffix);

   /* reallocate */
   *element = (char*) realloc(*element, sizeof(char) * length) ;
   /* alloc failed ? */
   if (! *element)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","recreateServersSupplementNames", "couldn't (re)allocate memory\n");
      return false;
   }

   /* set new service name */
   if (false == setRecordVariablesString(element, newName, title, 0))
   {
      message(stderr,__FILE__,__LINE__,"ERROR","recreateServersSupplementNames", "setRecordVariablesString failed\n");
      return false;
   }
   safeStringFree(&newName, length);
   return true;
}

bool supplementServersFormat(struct recordVariables *thisRecord)
{
   if ( NULL != thisRecord->dimStringTransportCommandFormat && NULL == thisRecord->dimStringTransportServiceFormat)
   {
      /* copy format from Command to Service */
      if ( NULL != thisRecord->dimStringTransportCommandFormat)
      {
         /* copy format from Command to Service */
         if (! setRecordVariablesString(&(thisRecord->dimStringTransportServiceFormat), thisRecord->dimStringTransportCommandFormat, "dimStringTransportServiceFormat", '\"'))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementServersFormat", "setRecordVariablesString failed\n");
            return false;
         }

         if (!disassembleFormat(thisRecord->dimStringTransportServiceFormat,
                                &thisRecord->dimStringTransportNumberOfServiceFormats,
                                &thisRecord->dimStringTransportServiceDataTypes,
                                &thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementServersFormat", "disassembleFormat() failed");
            return false;
         }
      }
      if ( NULL == thisRecord->dimStringTransportCommandFormat && NULL != thisRecord->dimStringTransportServiceFormat)
      {
         /* copy format from Service to Command */
         if (! setRecordVariablesString(&(thisRecord->dimStringTransportCommandFormat), thisRecord->dimStringTransportServiceFormat, "dimStringTransportCommandFormat", '\"'))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementServersFormat", "setRecordVariablesString failed\n");
            return false;
         }

         if (!disassembleFormat(thisRecord->dimStringTransportCommandFormat,
                                &thisRecord->dimStringTransportNumberOfCommandFormats,
                                &thisRecord->dimStringTransportCommandDataTypes,
                                &thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementServersFormat", "disassembleFormat() failed");
            return false;
         }
      }
   }
   return true;
}

bool supplementStringTransportServersFormat(struct recordVariables *thisRecord)
{
   if (( true == thisRecord->stringTransportCommand ) || (true == thisRecord->stringTransportService))
   {
      /* copy format from Command to Service */
      if ( NULL != thisRecord->dimStringTransportCommandFormat && NULL == thisRecord->dimStringTransportServiceFormat)
      {
         /* copy format from Command to Service */
         if (! setRecordVariablesString(&(thisRecord->dimStringTransportServiceFormat), thisRecord->dimStringTransportCommandFormat, "dimStringTransportServiceFormat", '\"'))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementStringTransportServersFormat", "setRecordVariablesString failed\n");
            return false;
         }

         if (!disassembleFormat(thisRecord->dimStringTransportServiceFormat,
                                &thisRecord->dimStringTransportNumberOfServiceFormats,
                                &thisRecord->dimStringTransportServiceDataTypes,
                                &thisRecord->dimStringTransportNumberOfServiceDataTypesPerFormat))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementStringTransportServersFormat", "disassembleFormat() failed");
            return false;
         }
      }
      if ( NULL == thisRecord->dimStringTransportCommandFormat && NULL != thisRecord->dimStringTransportServiceFormat)
      {
         /* copy format from Service to Command */
         if (! setRecordVariablesString(&(thisRecord->dimStringTransportCommandFormat), thisRecord->dimStringTransportServiceFormat, "dimStringTransportCommandFormat", '\"'))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementStringTransportServersFormat", "setRecordVariablesString failed\n");
            return false;
         }

         if (!disassembleFormat(thisRecord->dimStringTransportCommandFormat,
                                &thisRecord->dimStringTransportNumberOfCommandFormats,
                                &thisRecord->dimStringTransportCommandDataTypes,
                                &thisRecord->dimStringTransportNumberOfCommandDataTypesPerFormat))
         {
            message(stderr,__FILE__,__LINE__,"ERROR","supplementStringTransportServersFormat", "disassembleFormat() failed");
            return false;
         }
      }
   }
   return true;
}

bool setCombinedDnsAddressAndPort(struct recordVariables* thisRecord, char argument[])
{
   int i=0;
   char **argArray = NULL;
   int found_elements = 0;
   bool returnValue = true;

   /* local argument array of char pointers */
   argArray = NULL;

   found_elements = divideUpStrings (&argument[0], &argArray, ":", 2, DIM_TOKEN_PROTECTION);
   if (2 == found_elements || 1 == found_elements)
   {
      if (! setRecordVariablesString(&(thisRecord->dimDnsAddress), argArray[0], "dimDnsAddress", DIMDNSADDRESS) )
      {
         returnValue = false;
      }
      else
      {
         if (2 == found_elements)
         {
            if (0 <= thisRecord->dimDnsPort)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setCombinedDnsAddressAndPort", "dns port is already set to %i, what to choose? ... returning NULL\n",
                       thisRecord->dimDnsPort);
               return false;
            }
            else
            {
               if (false == checkValidUnsignedInt(argArray[1]))
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","setCombinedDnsAddressAndPort", "dns port is not set to a valid unsigned integer: `%s' ... returning false\n",
                          argArray[1]);
                  returnValue = false;
               }
               else
               {
                  thisRecord->dimDnsPort = atoi(argArray[1]);
               }
            }
         }
      }
   }
   else
   {
      message(stderr,__FILE__,__LINE__,"ERROR","setCombinedDnsAddressAndPort", "`%s' does contain %i `%s' only one is allowed, cannot be used ... returning false\n",
              argument,found_elements-(found_elements>1)?-1:0,":");
      returnValue = false;
   }

   /*clean up and free*/
   for (i = 0; i < found_elements; i++)
   {
      safeStringFree(&argArray[i],found_elements);
   }
   safePArrayFree((void**) argArray, found_elements);

   return returnValue;
}

bool setCombinedClientServiceScanTypeAndInterval(struct recordVariables* thisRecord, char argument[])
{
   int i=0;
   char **argArray = NULL;
   int found_elements = 0;
   bool returnValue = true;

   /* local argument array of char pointers */
   argArray = NULL;

   found_elements = divideUpStrings (&argument[0], &argArray, ":", 2, DIM_TOKEN_PROTECTION);
   if (2 == found_elements || 1 == found_elements)
   {
      switch(argArray[0][0])
      {
      case DIMSCANTYPETIMED:
         thisRecord->dimServiceScanType = TIMED;
         break;
      case DIMSCANTYPEMONITORED:
         thisRecord->dimServiceScanType = MONITORED;
         break;
      default:
         message(stderr,__FILE__,__LINE__,"ERROR","setCombinedClientServiceScanTypeAndInterval", "`%s' is not a valid scan mode only '%c' or '%c' are allowed ... returning false\n",
                 argArray[0],DIMSCANTYPETIMED, DIMSCANTYPEMONITORED);
         returnValue = false;
         break;
      }
      if (2 == found_elements)
      {
         if (0 <= thisRecord->dimServiceScanInterval)
         {
            message(stderr,__FILE__,__LINE__,"ERROR","setCombinedClientServiceScanTypeAndInterval", "clients service scan interval (timeout) already set to %i, what to choose? ... returning NULL\n",
                    thisRecord->dimServiceScanInterval);
            returnValue = false;
         }
         else
         {
            /* only floats are allowed, check for decimal points or commas */
            if (false == checkValidUnsignedInt(argArray[1]))
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setCombinedClientServiceScanTypeAndInterval", "clients service scan interval (timeout) is not set to an valid unsigned integer but something different `%s' ... returning false\n",
                       argArray[1]);
               returnValue = false;
            }
            else
            {
               thisRecord->dimServiceScanInterval=atoi(argArray[1]);
            }
         }
      }
   }
   else
   {
      message(stderr,__FILE__,__LINE__,"ERROR","setCombinedClientServiceScanTypeAndInterval", "`%s' does contain %i `%s' only one is allowed, cannot be used ... returning false\n",
              argument,found_elements-(found_elements>1)?-1:0,":");
      returnValue = false;
   }

   /*clean up and free*/
   for (i = 0; i < found_elements; i++)
   {
      safeStringFree(&argArray[i],found_elements);
   }
   safePArrayFree((void**) argArray, found_elements);

   return returnValue;
}

bool checkValidUnsignedInt(char argument[])
{
   if (NULL == argument)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","checkValidUnsignedInt", "argumentument == NULL ... return false");
      return false;
   }

   /* only ints are allowed, check for decimal points or commas */
   if ((NULL != strchr(argument,',')) || (NULL != strchr(argument,'.')))
   {
      return false;
   }
   /* check if argument is a number, i.e. its ASCII value is 48 ... 57 */
   if ( argument[0] < '0' || argument[0] > '9')
   {
      return false;
   }
   return true;
}

bool makeServerName(struct dbCommon* pRecord)
{
   extern int get_proc_name(char *);
   extern int get_node_name(char *);
   char proc[400];
   char node[400];
   char port[400];
   get_proc_name(proc);
   get_node_name(node);
   snprintf(port,sizeof(port),"%i", dis_get_dns_port());
   char nodePrefix[] = DIMSERVERNODEPREFIX;
   char pidPrefix[] = DIMSERVERPIDPREFIX;
   char nodePidSeparator[] = DIMSERVERNODEPIDSEPARATOR;
   char atNodeSeparator[] = DIMSERVERATNODESEPARATOR;
   char nodePortSeparator[] = DIMSERVERNODEPORTSEPARATOR;

   int serverNameLength = strlen(nodePrefix)
                          + strlen(nodePidSeparator)  + strlen(pidPrefix)
                          + strlen(proc) + strlen(atNodeSeparator) + strlen(node) +
                          + strlen(nodePortSeparator) + strlen(port)
                          +1;

   if ( 65536 < serverNameLength )
   {
      message(stderr,__FILE__,__LINE__,"ERROR","makeServerName()", "string lenght of serverName exceeds 64kB\n");
      return false;
   }

   serverName = (char*) calloc ( serverNameLength , sizeof(char));
   if (NULL == serverName)
   {
      message(stderr,__FILE__,__LINE__,"ERROR","makeServerName()", "cannot allocate memory for serverName\n");
      return false;
   }

   sprintf(&serverName[0],"%s%s%s%s%s%s%s%s",
           nodePrefix,nodePidSeparator,pidPrefix,proc,
           atNodeSeparator,node, nodePortSeparator, port);
   return true;
}

bool setDnsPort(struct recordVariables *thisRecord)
{
   /*
   * port:
   *  if value is set,
   *   then use this instead of default DIM_DNS_PORT
   */
   int port = -1;
   int returnValue = 0;

   if (-1 != thisRecord->dimDnsPort)
   {
      if (DIMSERVER == thisRecord->dimTierType)
      {
         if (false == dimServerDnsPortSet)
         {
            returnValue = 0;
            returnValue = dis_set_dns_port(thisRecord->dimDnsPort);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsPort()", "dis_set_dns_port failed\n");
               return false;
            }
            dimServerDnsPortSet = true;
         }
         else
         {
            port = -1;
            port = dis_get_dns_port();

            if (port != thisRecord->dimDnsPort)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsPort()", "servers' port is already set to %i, cannot set %i\n",
                       port, thisRecord->dimDnsPort);
               return false;
            }
         }
      }
      else /*CLIENT*/
      {
         if (false == dimClientDnsPortSet)
         {
            returnValue = 0;
            returnValue = dic_set_dns_port(thisRecord->dimDnsPort);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsPort()", "dic_set_dns_port failed\n");
               return false;
            }
            dimClientDnsPortSet = true;
         }
         else
         {
            port = -1;
            port = dic_get_dns_port();

            if (port != thisRecord->dimDnsPort)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsPort()", "clients' port is already set to %i, cannot set %i\n",
                       port, thisRecord->dimDnsPort);
               return false;
            }
         }
      }
   }
   else /*(-1 == thisRecord->dimDnsPort)*/
   {
      /*
      * cancel if environment variable is not set
      */
      if ( DIMSERVER == thisRecord->dimTierType)
      {
         port = dis_get_dns_port();
      }
      else /*CLIENT*/
      {
         port = dic_get_dns_port();
      }

      /*
      * set dns port to struct
      */

      thisRecord->dimDnsPort = port;
      /* setting globals */
      if ( DIMSERVER == thisRecord->dimTierType)
      {
         dimServerDnsPortSet = true;
      }
      else /*CLIENT*/
      {
         dimClientDnsPortSet = true;
      }
   }
   return true;
}

bool setDnsAddress(struct recordVariables *thisRecord)
{
   /*
   * node:
   *  if value is set,
   *   then use this instead of default DIM_DNS_NODE
   */
   int returnValue = 0;
   char dns_node_name[255] = {'\0'};
   if (NULL != thisRecord->dimDnsAddress)
   {
      if (DIMSERVER == thisRecord->dimTierType)
      {
         if (false == dimServerDnsAddressSet)
         {
            returnValue = 0;
            returnValue = dis_set_dns_node(thisRecord->dimDnsAddress);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dis_set_dns_node failed\n");
               return false;
            }
            dimServerDnsAddressSet = true;
         }
         else
         {
            returnValue = 0;
            returnValue = dis_get_dns_node(dns_node_name);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dis_get_dns_node failed\n");
               return false;
            }

            /* check if dimDnsAdress is already part of dns_node_name */
            if (NULL == strstr(dns_node_name,&thisRecord->dimDnsAddress[0]))
            {
               /* attach at the end of the search path*/
               /* warning */
               message(stderr,__FILE__,__LINE__,"WARNING","setDnsAdress()", "servers' dim Dns Address already set to `%s', attaching `%s' at the end\n",
                       dns_node_name, thisRecord->dimDnsAddress);

               /* check maximum length */
               if ( MAXIMUMDIMDNSADDRESSLENGTH < strlen(dns_node_name) + strlen(",") + strlen(thisRecord->dimDnsAddress)) /*string too long*/
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","setDnsAdress()", "`%s,%s' is too long, max length: %i\n",
                          dns_node_name,thisRecord->dimDnsAddress, MAXIMUMDIMDNSADDRESSLENGTH);
                  return false;
               }

               /* attach at the end of the search path*/
               strcat(dns_node_name, ",");
               strcat(dns_node_name, thisRecord->dimDnsAddress);
               returnValue = 0;
               returnValue = dis_set_dns_node(dns_node_name);
               if (1 != returnValue)
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dis_set_dns_node failed\n");
                  return false;
               }
            }
         }
      }
      else /*CLIENT*/
      {
         if (false == dimClientDnsAddressSet)
         {
            returnValue = 0;
            returnValue = dic_set_dns_node(thisRecord->dimDnsAddress);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dic_set_dns_node failed\n");
               return false;
            }
            dimClientDnsAddressSet = true;
         }
         else
         {
            returnValue = 0;
            returnValue = dic_get_dns_node(dns_node_name);
            if (1 != returnValue)
            {
               message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dic_get_dns_node failed\n");
               return false;
            }

            /* check if dimDnsAdress is already part of dns_node_name */
            if (NULL == strstr(dns_node_name,&thisRecord->dimDnsAddress[0]))
            {
               /* attach at the end of the search path*/
               /* warning */
               message(stderr,__FILE__,__LINE__,"WARNING","setDnsAdress()", "clients' dim Dns Address already set to `%s', attaching `%s' at the end",
                       dns_node_name, thisRecord->dimDnsAddress);

               /* check maximum length */
               if ( MAXIMUMDIMDNSADDRESSLENGTH < strlen(dns_node_name) + strlen(",") + strlen(thisRecord->dimDnsAddress)) /*string too long*/
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","setDnsAdress()", "`%s,%s' is too long, max length: %i\n",
                          dns_node_name,thisRecord->dimDnsAddress, MAXIMUMDIMDNSADDRESSLENGTH);
                  return false;
               }

               /* attach at the end of the search path*/
               strcat(dns_node_name, ",");
               strcat(dns_node_name, thisRecord->dimDnsAddress);
               returnValue = 0;
               returnValue = dic_set_dns_node(dns_node_name);
               if (1 != returnValue)
               {
                  message(stderr,__FILE__,__LINE__,"ERROR","setDnsAddress()", "dic_set_dns_node failed\n");
                  return false;
               }
            }
         }
      }
   }
   else /*(NULL == thisRecord->dimDnsAddress)*/
   {
      /*
      * cancel if environment variable is not set
      */
      returnValue = 0;
      if ( DIMSERVER == thisRecord->dimTierType)
      {
         returnValue = dis_get_dns_node( dns_node_name );
      }
      else /*CLIENT*/
      {
         returnValue = dic_get_dns_node( dns_node_name );
      }

      if( (0 == strcmp(dns_node_name, "")) ||  (1 != returnValue ) )
      {
         message(stderr,__FILE__,__LINE__,"ERROR","setDnsAdress()", "no dns address\n");
         return false;
      }

      /*
      * set dns address to struct
      */
      thisRecord->dimDnsAddress = (char*) malloc (sizeof(char) * (1 +  strlen(dns_node_name)));
      strncpy(thisRecord->dimDnsAddress, dns_node_name, (1+strlen(dns_node_name)));
      /* setting globals */
      if ( DIMSERVER == thisRecord->dimTierType)
      {
         dimServerDnsAddressSet = true;
      }
      else /*CLIENT*/
      {
         dimClientDnsAddressSet = true;
      }
   }

   /*
   * TODO: test, if a real DNS exists, not just the environment variable
   */
   return true;
}

int errorDeactivateRecord(struct dbCommon *pRecord, int line, char* fcn, char* format, ... )
{

   va_list argumentPointer;
   va_start(argumentPointer, format);
   char *errorstring = (char*) calloc(1000, sizeof(char));
   vsnprintf(errorstring, 1000, format, argumentPointer);

   message(stderr, __FILE__, line, "ERROR", fcn, "%s ... setting PACT=1 (deactivating) for record `%s'\n",
           errorstring, pRecord->name);

   pRecord->pact = 1;
   /* TODO: raise an EPICS alarm */

   va_end(argumentPointer);
   free(errorstring);

   return 0;
   /* or better return 2 ??? */
}

