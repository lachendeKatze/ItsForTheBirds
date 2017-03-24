/*
* "It's For the Birds!"
* Greg and Philip
* Node/Javascript code for the Intel Edison
*
*/

// all the awesome libraries!
var noble        = require('noble');
var PubNub       = require('pubnub');
var cloudinary   = require('cloudinary');
var childProcess = require('child_process');

// BLE Service and Characteristics for noble
// These should correspond to the peripheral's service and characteristic UUIDs
// ( on the Arduino 101 in this case)
var LOCAL_NAME          = 'bird';
var SERVICE_UUID        = '917649a0d98e11e59eec0002a5d5c51b'; //no dashes!!!!
var PERCH_UUID          = '917649a1d98e11e59eec0002a5d5c51b';
var TEMPERATURE_UUID    = '917649a2d98e11e59eec0002a5d5c51b';
var HUMIDITY_UUID       = '917649a3d98e11e59eec0002a5d5c51b';
var FEED_UUID           = '917649a4d98e11e59eec0002a5d5c51b';

// PubNub object created here
pubnub = new PubNub({
    subscribeKey: 'your key here',
    publishKey: 'your key here'
});

// cloudinary object for image storage and manipulation created hereby
// add your particulars here
cloudinary.config({
    cloud_name: '',
    api_key: '',
    api_secret: ''
});

// bird feeder variables
var temperature, humidity, feedLevel;
var countNotifications = 0;
var leftPerch = 0;
var rightPerch = 0;
var totalPerchCount = 0;

// file state and counters
var fileCount = 0;
var fileUploadCount = 0;
var lastFileSent = 0;
var fileName = '/media/sdcard/test04_'; // this is a bad variable name!
var lastestUploaded = 'none';
var lastThreeImages = ['none','none','none'];
var image1, image2, image3;

//instantiate BLE, make sure you enable BT with 'rfkill unblock bluetooth'
noble.on('stateChange', function(state){
	if(state === 'poweredOn'){
		noble.startScanning();
		console.log('Scanning for BLE peripherals...');
	}else{
		noble.stopScanning();
	}
});



/*
* This is a very busy function and should be broken  done in future versions for
* greater readability/debugging
*/
function transformRawData(characteristicuuid, data)
{
    var dataBuffer = new Buffer(data);
    if (characteristicuuid == PERCH_UUID)
    {
        console.log('perch: ' + data.readUInt8(0));
        var perchData = data.readUInt8(0);
        if (perchData == 1){leftPerch++}
        else {rightPerch++}
        totalPerchCount++;
        fileName = fileName + fileCount.toString() + '.jpg';
        console.log("File Name: " + fileName);
        childProcess.execFile('fswebcam',['-r 1280x720',fileName],function(error,stdout,stderr)
        {
            console.log(stdout);
        });
        fileCount++;
        fileUploadCount++;
        fileName = '/media/sdcard/test04_';
    }
    else if (characteristicuuid == TEMPERATURE_UUID)
    {
        temperature = data.readFloatLE(0);
        countNotifications++;
        console.log('temperature: ' + temperature);
    }
    else if(characteristicuuid == HUMIDITY_UUID)
    {
        humidity = data.readFloatLE(0);
        countNotifications++;
        console.log('humidity: ' + humidity );
    }
    else if(characteristicuuid == FEED_UUID)
    {
        feedLevel = dataBuffer.readInt16LE(0);
        countNotifications++;
        console.log('feed level:' + feedLevel);
    }
    else {console.log("unknown uuid!");}

    if(countNotifications == 3)
    {
        publishFeederData();
        publishFeederImages();
        countNotifications = 0;
    }

    if (fileUploadCount > 3)
    {
        console.log("file upload count: " + fileUploadCount );
        for( i=0;i<3;i++)
        {

            var countTag = lastFileSent++;
            var fileToSendName = "/media/sdcard/test04_" + lastFileSent.toString() + ".jpg";
            console.log("index: " + i);
           // cloudinary.uploader.upload(fileToSendName, function(result){
                // console.log(result);
               //   console.log("The URL for the image is: " + result["url"]);
            //    lastestUploaded = result["url"];
              //  lastThreeImages.push(result["url"]);
            // });

            console.log("image url array:" + lastThreeImages[i]);
            console.log("File Path & Name: " + fileToSendName);
        }
        fileUploadCount = 0;
    }

    // debugging
    // console.log("notifications: " + countNotifications);
}

noble.on('discover', function(peripheral){
    console.log('Found BLE Device: [' + peripheral.id + '] ' + peripheral.advertisement.localName);
    if(peripheral.advertisement.localName == LOCAL_NAME){
		  console.log('Found: ' + peripheral.advertisement.localName);
    }
    peripheral.connect(function(error)
    {
        console.log('Connected to peripheral: ' + peripheral.uuid);
        noble.stopScanning(); // prevent us from picking up "stray" services
        peripheral.discoverServices([SERVICE_UUID], function(error, services) {
            console.log('services: ' + services.length);
            var feederService = services[0];
            console.log('Bird Feeder Service!');

            feederService.discoverCharacteristics([], function(error, characteristics) {
                characteristics.forEach(function(characteristic) {
                    console.log('characteristic UUID: ' + characteristic.uuid);
                    characteristic.on('data', function(data, isNotification) {
                        transformRawData(characteristic.uuid,data);
                    });
                    characteristic.notify('true', function(error) { if (error) throw error; });
                });
            });
        });
    });
});


function publishFeederImages()
{
  for(i=0;i<3;i++)
    {
      pubnub.publish({
        channel: 'feederView',
        message: {
            'imageIndex': i,
            'latestView': lastThreeImages[i]
        }
      });
    }

    for(i=0;i<3;i++)
    {
        lastThreeImages.pop();
    }

}

function publishFeederData()
{
    console.log("Publishing bird feeder data to PubNub(just t)");
    pubnub.publish({
        channel:'environment',
        message:
        {
            eon:
            {
                'temperature': temperature,
                'humidity': humidity,
            }

        }
    });
    pubnub.publish({
        channel:'feeder',
        message:
        {
            eon:
            {
                'feedLevel': feedLevel
            }

        }
    });
    pubnub.publish({
        channel:'perch',
        message:
        {
            eon:
            {
                'leftPerch': leftPerch,
                'rightPerch': rightPerch,
                'total': totalPerchCount
            }

        }
    });
}
