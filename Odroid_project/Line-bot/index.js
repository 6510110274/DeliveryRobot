const line = require('@line/bot-sdk')
const express = require('express')
const axios = require('axios').default
const dotenv = require('dotenv')
const { exec } = require('child_process');

const env = dotenv.config().parsed
const app = express()

let userMap = {}; // ออบเจกต์เพื่อเก็บ username:UserID
let targetUserIdForLocation = null;
let messageText
let currentlat;
let currentlon;
let destlat;
let destlon;


function runPythonScript(currentlat,currentlon,destlat,destlon) {
    const pythonScript = `python3 /home/odroid/Embedded/send_gps.py ${currentlat} ${currentlon} ${destlat} ${destlon}`;
    exec(pythonScript, (error, stdout, stderr) => {
        if (error) {
            console.error(`Error executing Python script: ${error}`);
            return;
        }
        if (stderr) {
            console.error(`Python stderr: ${stderr}`);
        }
        console.log(`Python stdout: ${stdout}`);
    });
}

const lineConfig = {
    channelAccessToken: env.ACCESS_TOKEN,
    channelSecret: env.SECRET_TOKEN
}

const client = new line.Client(lineConfig);

app.post('/webhook', line.middleware(lineConfig), async (req, res) => {
    try{
        const events = req.body.events
        //console.log('event=>>>>',events)
        return events.length > 0 ? await events.map(item => handleEvent(item)) : res.status(200).send("OK")
    }catch(err){
        res.status(500).end()
    }
});

const handleEvent = async(event) =>{

    const userId = event.source.userId;
    console.log("User ID:", userId);

    if(event.source.userId === env.USER_ID_PAPUN){
        console.log("Message from admin:", event.message);
        
        if(event.message.type === 'location'){
            currentlat = event.message.latitude
            currentlon = event.message.longitude
            return client.replyMessage(event.replyToken,{
                type: 'text',
                text: `เราได้บันทึกพิกัด Latitude:${currentlat} , Longitude: ${currentlon} เรียบร้อยแล้ว`
            })
        }

        messageText = event.message.text;

        if(messageText.startsWith('Send GPS')){
            runPythonScript(currentlat,currentlon,destlat,destlon);
            return client.replyMessage(event.replyToken,{
                type: 'text',
                text: `ทำการส่งGPSไปยังรถเรียบร้อย`
            })
        }
        else if (messageText.startsWith('gift to:')) {
            const username = messageText.split('gift to:')[1].trim();
            const targetUserId = userMap[username]; // หาค่า userID ของ username ที่ถูกส่งมา

            if (targetUserId) {
                // อัปเดตค่า targetUserIdForLocation ให้เป็น userID ของ username นั้น
                targetUserIdForLocation = targetUserId;
                console.log(`Admin assigned delivery to: ${username} (${targetUserId})`);
                await client.pushMessage(targetUserId, {
                    type: 'text',
                    text: 'Robot is wait you send location'
               });
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `Waiting for location from ${username} (${targetUserId})`
                });
            } else {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `ไม่พบ username: ${username}`
                });
            }
        }
        // ตรวจสอบว่า admin ส่งคำว่า "get IDs"
        else if (event.message.text === 'list username') {
            let userList = '';

            // สร้างข้อความที่มีรายการของ username และ userID ทั้งหมด
            for (const username in userMap) {
                userList += `${username}: ${userMap[username]}\n`;
            }

            if (userList) {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `User list:\n${userList}`
                });
            } else {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: 'No users found!'
                });
            }
        }
        // ตัวอย่าง admin ส่งคำว่า "ขอ userID ของ @username"
        else if (messageText.startsWith('ขอ userID ของ ')) {
            const username = messageText.split('ขอ userID ของ ')[1].trim();
            const targetUserId = userMap[username];

            if (targetUserId) {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `UserID ของ ${username} คือ ${targetUserId}`
                });
            } else {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `ไม่พบ userID ของ ${username}`
                });
            }
        }
        else{
            return client.replyMessage(event.replyToken, {
                type: 'text',
                text: 'Invalid UserID format! Please use "gift to:<username>"'
            });
        } 
    }
    if (event.type === 'message' && event.message.type === 'text') {
        const messageText = event.message.text.trim();

        // ตรวจสอบว่าข้อความขึ้นต้นด้วย "username:"
        if (messageText.startsWith('username:')) {
            // ดึงค่าที่อยู่หลัง "username:"
            const username = messageText.split('username:')[1].trim();
            const userId = event.source.userId;

            // ตรวจสอบว่า userId นี้เคยลงทะเบียนด้วย username มาก่อนหรือไม่
            const existingUsernameForUserId = Object.entries(userMap).find(([key, value]) => value === userId)?.[0];

            // ตรวจสอบว่า userId นี้เคยลงทะเบียนด้วย username มาก่อนหรือไม่
            if (existingUsernameForUserId) {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `You have already registered with username: ${existingUsernameForUserId}`
                });
            }
    
            // ตรวจสอบว่า username นี้มีคนลงทะเบียนหรือยัง
            if (userMap[username]) {
                return client.replyMessage(event.replyToken, {
                    type: 'text',
                    text: `This username: ${username} is already taken.`
                });
            }
    
            // เก็บ userId และ username ใน userMap
            userMap[username] = userId;
            console.log(`Stored: ${username} => ${userId}`);

            await client.pushMessage(env.USER_ID_PAPUN, {
                type: 'text',
                text: `client register delivery username: ${username}`
           });
    
            return client.replyMessage(event.replyToken, {
                type: 'text',
                text: `Username ${username} has been stored successfully!`
            });
        } else {
            return client.replyMessage(event.replyToken, {
                type: 'text',
                text: 'Please send the username in the format "username:<your_username>".'
            });
        }
    }
        
    // }
    if (event.type === 'message' && event.message.type === 'location' && userId === targetUserIdForLocation) {
        // ดึงข้อมูล location จาก message
        destlat = event.message.latitude;
        destlon = event.message.longitude;
    
        // ส่ง location ขึ้นไปยัง ThingSpeak
        await axios.get(`https://api.thingspeak.com/update`, {
            params: {
                api_key: env.THING_API, // API key ของ ThingSpeak
                field1: destlat,
                field2: destlon
            }
        });
    
        console.log(`Location sent to ThingSpeak: lat=${destlat}, lon=${destlon}`);
    
        // รันสคริปต์ Python เพื่อส่งข้อมูล GPS ไปยัง Arduino
        //runPythonScript(latitude, longitude);
        runPythonScript()
        // ตอบกลับไปยัง user
        return client.replyMessage(event.replyToken, {
            type: 'text',
            text: `Location successfully sent!`
        });
    }

    // หากผู้ใช้อื่นส่ง location มา แต่ไม่ตรงกับ userID ที่ admin ระบุ
    else if (event.type === 'message' && event.message.type === 'location') {
        console.log(`${username} try to send location`)
        return client.replyMessage(event.replyToken, {
            type: 'text',
            text: 'Location received, but you are not the user being tracked.'
        });
    }

    if (event.type === 'message' && event.message.type === 'sticker') {
        const stickerId = event.message.stickerId;
        const packageId = event.message.packageId;

        console.log(`Received sticker: packageId=${packageId}, stickerId=${stickerId}`);

        // ตอบกลับด้วยสติ๊กเกอร์
        return client.replyMessage(event.replyToken, {
            type: 'text',
            text: `Thank You`
        });
    }

    if(event.type !== 'message' || event.message.type !== 'location'){
        console.log(event)
        return null;
    }
}

app.listen(8080, () => {
    console.log('listening on 8080')
})