#ifndef DEVCADIMINTERFACESOFT_H_
#define DEVCADIMINTERFACESOFT_H_

void error_routine (int severity, int error_code, char* message);
void client_exit_routine (int *tag);
void exit_routine (int *code);
bool makeServerName(struct dbCommon* pRecord);
int errorDeactivateRecord(struct dbCommon *pRecord, int line, char* fcn, char* format, ... );

char* setRecordVariablesString(char **element, char argument[], char discription[], char key);

typedef struct recordVariables
{
   void *dimData;
   unsigned int dataSize;
   void *recordAddress;
   unsigned int   index;
   int   dimCommandServiceID;
   int   dimServiceServiceID;
   char  dimTierType;
   int   calledFromCallback;
   char *dimCommandName;
   char *dimServiceName;
   char *dimCommandFormat;
   char *dimServiceFormat;
   char *dimServiceDataTypes;
   char *dimCommandDataTypes;
   unsigned int dimNumberOfServiceFormats;
   unsigned int dimNumberOfCommandFormats;
   unsigned int *dimNumberOfServiceDataTypesPerFormat;
   unsigned int *dimNumberOfCommandDataTypesPerFormat;
   char *dimDnsAddress;
   int   dimDnsPort;
   char *recordType;
   int   recordTypeIndex;
   char *dimServicesNamePrefix;
   bool  dimServiceExists;
   bool  dimCommandExists;
   char* dimGetDefaultSuffix;
   char* dimPutDefaultSuffix;
   int dimServiceScanType;
   int dimServiceScanInterval;
   bool clientServiceCallbackOk;
   bool serverCommandCallbackOk;

   bool stringTransportCommand;
   bool stringTransportService;
   bool stringTransportBoth;
   char *dimStringTransportCommandFormat;
   char *dimStringTransportServiceFormat;
   char *dimStringTransportServiceDataTypes;
   char *dimStringTransportCommandDataTypes;
   unsigned int dimStringTransportNumberOfServiceFormats;
   unsigned int dimStringTransportNumberOfCommandFormats;
   unsigned int *dimStringTransportNumberOfServiceDataTypesPerFormat;
   unsigned int *dimStringTransportNumberOfCommandDataTypesPerFormat;

   bool dimCreateBoth;
}
recordVariables;
/*recordVariables wenn ich das hier an und aus mache erscheint im outline irgendwie ein gelbes T Symbol.
 * Was ist der Unterschied??*/

void dumpStruct ( struct recordVariables *rec, FILE *stream );
recordVariables* createAndInitVariables( struct dbCommon	*pRecord );
void clearStructRecordVariables( struct recordVariables *thisRecord );
epicsEnum16 determineRecordType(struct dbCommon* pRecord);
bool checkValidType(char *type);
bool disassembleFormat(char formatString[],
                       unsigned int* dimNumberOfCommandFormats,
                       char** dimCommandDataTypes,
                       unsigned int** dimNumberOfCommandDataTypesPerFormat );
bool supplementServersNamesAndFormats(struct recordVariables *thisRecord);
bool supplementServersFormat(struct recordVariables *thisRecord);
bool supplementStringTransportServersFormat(struct recordVariables *thisRecord);
bool recreateServersSupplementNames(char** element, char name[], char suffix[], char title[]);
bool setCombinedDnsAddressAndPort(struct recordVariables* thisRecord, char argumentArray[]);
bool setCombinedClientServiceScanTypeAndInterval(struct recordVariables* thisRecord, char argument[]);
bool checkValidUnsignedInt(char arg[]);
bool setDnsAddress(struct recordVariables *thisRecord);
bool setDnsPort(struct recordVariables *thisRecord);
long copyEpicsToDimData(struct recordVariables *thisRecord);
long copyDimToEpicsData(struct recordVariables *thisRecord);
unsigned int calculateDataSize(unsigned int numberOfFormats,
                               char dataTypes[],
                               unsigned int numberOfDataTypesPerFormat[]);
unsigned int determineDataSize(struct recordVariables *thisRecord);
bool checkConsistencyFormatTypes(struct recordVariables *thisRecord);
bool checkConsistencyRecordSpecific(struct recordVariables *thisRecord);
bool addServersPrefix(struct recordVariables *thisRecord);
bool setServersDefaultDataFormat(struct recordVariables *thisRecord);
bool getServiceFormat(char *serviceFormat, struct recordVariables *thisRecord);
bool getCommandFormat(char *serviceFormat, struct recordVariables *thisRecord);
bool retrieveAndSetClientServicesFormat(struct recordVariables *thisRecord);

typedef enum {
   DIMaiRecord
   ,DIMaoRecord
   ,DIMbiRecord
   ,DIMboRecord
   ,DIMmbbiRecord
   ,DIMmbboRecord
   ,DIMmbbiDirectRecord
   ,DIMmbboDirectRecord
   ,DIMstringinRecord
   ,DIMstringoutRecord
   ,DIMlonginRecord
   ,DIMlongoutRecord
   ,DIMsubArrayRecord
   ,DIMwaveformRecord
   /*	,DIMgenSubRecord */
   ,DIMLastDummyEntry
} DIMInterfaceSupportedRecords;

char *DIMInterfaceSupportedRecordNames [DIMLastDummyEntry] =
   {
      "ai"
      ,"ao"
      ,"bi"
      ,"bo"
      ,"mbbi"
      ,"mbbo"
      ,"mbbiDirect"
      ,"mbboDirect"
      ,"stringin"
      ,"stringout"
      ,"longin"
      ,"longout"
      ,"subArray"
      ,"waveform"
      /*	,"genSub" */
   };

typedef enum {
   DIMDataTypeChar
   ,DIMDataTypeInt
   ,DIMDataTypeLong
   ,DIMDataTypeShort
   ,DIMDataTypeDouble
   ,DIMDataTypeFloat
   ,DIMDataTypeXtraLong
   ,DIMDataTypeLastDummyEntry
} DIMInterfaceSupportedDIMDataTypes;

char const DIMInterfaceSupportedDIMDataTypesNames [ DIMDataTypeLastDummyEntry ] =
   {
      'C'
      ,'I'
      ,'L'
      ,'S'
      ,'D'
      ,'F'
      ,'X'
   };

#endif /*DEVCADIMINTERFACESOFT_H_*/
