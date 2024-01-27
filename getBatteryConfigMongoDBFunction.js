exports = function({ query, headers, body}, response) {
    const {arg1, arg2} = query;
    const contentTypes = headers["Content-Type"];
    const reqBody = body;
    console.log("arg1, arg2: ", arg1, arg2);
    console.log("Content-Type:", JSON.stringify(contentTypes));
    console.log("Request body:", reqBody);
    payload = JSON.parse(reqBody.text())
    return context.services.get("mongodb-atlas").db("Batterybackup").collection("BatteryManagement").findOne({'mac_address':payload['mac_address']})
};