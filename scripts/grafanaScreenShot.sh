#!/bin/bash

APIKEY=eyJrIjoid25Bc3U3NEdxVDZYV2xrdEJ0QTlxNTdFM25JcXVTTlAiLCJuIjoiYWRtaW5LRVkiLCJpZCI6MX0=
url=http://localhost:3000

############# successfull single panel screenshot
#dashboardID=solaris-main-dashoard
#panelID=2
#curl "$url/render/d-solo/iXH6cG-Vz/$dashboardID?orgId=1&refresh=5s&from=now-6h&to=now&panelId=$panelID" -H "Authorization: Bearer ${APIKEY}" --compressed > haha.png

############# get list of snapshot
SNAPSHOT_LIST=$(curl -X GET -H "Authorization: Bearer ${APIKEY}" $url/api/dashboard/snapshots)

############ list the key
SNAPSHOT_KEYS=$(echo $SNAPSHOT_LIST | jq -r '.[].key')
#echo $SNAPSHOT_KEYS
echo "========== list of key"
for haha in $SNAPSHOT_KEYS
do
  echo $haha
done
echo "============================="

#############  delete snapshot
for haha in $SNAPSHOT_KEYS
do
  echo "Deleting snapshot $haha"
  curl -s -X DELETE -H "Authorization: Bearer $APIKEY" "$url/api/snapshots/$haha"
done
echo ""
echo "==========================="
curl -X GET -H "Authorization: Bearer ${APIKEY}" $url/api/dashboard/snapshots

############ reset the snapshot count
# # Set the new snapshot count
# NEW_SNAPSHOT_COUNT=0
# DASHBOARD_UID="iXH6cG-Vz"
# # Get the current dashboard settings
# DASHBOARD_SETTINGS=$(curl -s -H "Authorization: Bearer $APIKEY" "$url/api/dashboards/uid/$DASHBOARD_UID")
# # Update the snapshot count in the dashboard settings
# UPDATED_DASHBOARD_SETTINGS=$(echo "$DASHBOARD_SETTINGS" | jq ".dashboard.snapshotData.snapshotCount = $NEW_SNAPSHOT_COUNT")
# # Send the updated dashboard settings back to Grafana
# curl -s -X POST -H "Authorization: Bearer $APIKEY" -H "Content-Type: application/json" -d "$UPDATED_DASHBOARD_SETTINGS" "$url/api/dashboards/db"


############ get snapshot by key
#curl -X GET -H "Authorization: Bearer ${APIKEY}" $url/api/snapshots/gH0GK86ExXR0zr5rNhSUaweyxVH607BH

############ Generate snapshot [FAIL!!!!!!!!!!]
# SNAPSHOT_FILE=haha.png

# SNAPSHOT_RESULT=$(curl -s -X POST -H "Authorization: Bearer $APIKEY" -H "Content-Type: application/json" -d "@snapshot-data.json" "$url/api/snapshots")
# echo "========================"
# echo $SNAPSHOT_RESULT

# SNAPSHOT_URL=$(echo $SNAPSHOT_RESULT | jq -r '.url')
# echo "========================"
# echo $SNAPSHOT_URL

# #microsoft-edge $SNAPSHOT_URL

# microsoft-edge --headless --window-size=2000,1000 --screenshot=$SNAPSHOT_FILE $SNAPSHOT_URL

# #echo "Snapshot saved to $SNAPSHOT_FILE"

# #eog $SNAPSHOT_FILE

echo ""