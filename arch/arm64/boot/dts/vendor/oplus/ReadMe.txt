DTS修改规则：

以wudang项目为例：

1.	QCOM 路径下的所有修改保持原生态（升级codebase时会覆盖掉）

2.	对应原生lagoon.dtsi的修改：
1）平台共性的修改放到lagoon-oem-common.dtsi
2）项目特有的新建dtsi并包含到lagoon-wudang.dts

3.	对应原生的lagoon-mtp.dtsi的修改：
1）平台共性的修改放到lagoon-mtp-common-overlay.dtsi
2）项目特有的新建dtsi并包含到lagoon-mtp-wudang-overlay.dts，例如lagoon-mtp-wudang-overlay.dtsi
3）各模块新建的overlay dtsi包含在 lagoon-mtp-wudang-overlay.dts中