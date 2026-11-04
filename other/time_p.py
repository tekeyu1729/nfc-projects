import datetime
now = datetime.datetime.now()
time = list(map(int, now.strftime('%H %M').split(" ")))
print(f"{time[0]-1}:{time[1]+55}" if 0<=time[1] and time[1]<=4 else f"{time[0]}:{str(float(time[1]-5)/10).replace(".","")}")
